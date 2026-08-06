#include "ParsedText.h"

#include <GfxRenderer.h>
#include <Utf8.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

#include "hyphenation/Hyphenator.h"

constexpr int MAX_COST = std::numeric_limits<int>::max();

namespace {

// Soft hyphen byte pattern used throughout EPUBs (UTF-8 for U+00AD).
constexpr char SOFT_HYPHEN_UTF8[] = "\xC2\xAD";
constexpr size_t SOFT_HYPHEN_BYTES = 2;

// Em-space (U+2003) UTF-8 bytes; used as a fallback paragraph indent.
constexpr char EM_SPACE_UTF8[] = "\xe2\x80\x83";
constexpr size_t EM_SPACE_BYTES = 3;

// Parser words are capped at 200 bytes. Paragraph indent and hyphenation add only
// a few bytes, so this keeps C API boundaries null-terminated without heap churn.
constexpr size_t WORD_NULL_TERM_BUF = 256;

const char* nullTerminate(char (&buf)[WORD_NULL_TERM_BUF], const std::string_view word) {
  const size_t copyLen = std::min(word.size(), WORD_NULL_TERM_BUF - 1);
  if (copyLen > 0) {
    std::memcpy(buf, word.data(), copyLen);
  }
  buf[copyLen] = '\0';
  return buf;
}

// Returns the first rendered codepoint of a word (skipping leading soft hyphens).
uint32_t firstCodepoint(const std::string_view word) {
  if (word.empty()) return 0;
  char buf[WORD_NULL_TERM_BUF];
  const auto* ptr = reinterpret_cast<const unsigned char*>(nullTerminate(buf, word));
  while (true) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) return 0;
    if (cp != 0x00AD) return cp;  // skip soft hyphens
  }
}

// Returns the last codepoint of a word by scanning backward for the start of the last UTF-8 sequence.
uint32_t lastCodepoint(const std::string_view word) {
  if (word.empty()) return 0;
  // UTF-8 continuation bytes start with 10xxxxxx; scan backward to find the leading byte.
  size_t i = word.size() - 1;
  while (i > 0 && (static_cast<uint8_t>(word[i]) & 0xC0) == 0x80) {
    --i;
  }
  char buf[8];
  const size_t copyLen = std::min(word.size() - i, sizeof(buf) - 1);
  std::memcpy(buf, word.data() + i, copyLen);
  buf[copyLen] = '\0';
  const auto* ptr = reinterpret_cast<const unsigned char*>(buf);
  return utf8NextCodepoint(&ptr);
}

bool containsSoftHyphen(const std::string_view word) {
  return word.find(std::string_view(SOFT_HYPHEN_UTF8, SOFT_HYPHEN_BYTES)) != std::string_view::npos;
}

// Removes every soft hyphen in-place so rendered glyphs match measured widths.
void stripSoftHyphensInPlace(std::string& word) {
  size_t pos = 0;
  while ((pos = word.find(SOFT_HYPHEN_UTF8, pos, SOFT_HYPHEN_BYTES)) != std::string::npos) {
    word.erase(pos, SOFT_HYPHEN_BYTES);
  }
}

const char* sanitizeWord(char (&outBuf)[WORD_NULL_TERM_BUF], const std::string_view word, const bool appendHyphen) {
  size_t outLen = 0;
  for (size_t i = 0; i < word.size() && outLen + 1 < WORD_NULL_TERM_BUF;) {
    if (i + 1 < word.size() && static_cast<uint8_t>(word[i]) == 0xC2 &&
        static_cast<uint8_t>(word[i + 1]) == 0xAD) {
      i += SOFT_HYPHEN_BYTES;
      continue;
    }
    outBuf[outLen++] = word[i++];
  }
  if (appendHyphen && outLen + 1 < WORD_NULL_TERM_BUF) {
    outBuf[outLen++] = '-';
  }
  outBuf[outLen] = '\0';
  return outBuf;
}

// Returns the advance width for a word while ignoring soft hyphen glyphs and optionally appending a visible hyphen.
// Uses advance width (sum of glyph advances + kerning) rather than bounding box width so that italic glyph overhangs
// don't inflate inter-word spacing.
uint16_t measureWordWidth(const GfxRenderer& renderer, const int fontId, const std::string_view word,
                          const EpdFontFamily::Style style, const bool appendHyphen = false) {
  if (word.size() == 1 && word[0] == ' ' && !appendHyphen) {
    return renderer.getSpaceWidth(fontId, style);
  }
  const bool hasSoftHyphen = containsSoftHyphen(word);
  char buf[WORD_NULL_TERM_BUF];
  const char* cstr = (hasSoftHyphen || appendHyphen) ? sanitizeWord(buf, word, appendHyphen) : nullTerminate(buf, word);
  return renderer.getTextAdvanceX(fontId, cstr, style);
}

bool isCjkCodepointForSpacing(const uint32_t cp) {
  return (cp >= 0x2E80 && cp <= 0x2EFF) || (cp >= 0x3000 && cp <= 0x303F) ||
         (cp >= 0x3040 && cp <= 0x309F) || (cp >= 0x30A0 && cp <= 0x30FF) ||
         (cp >= 0x3100 && cp <= 0x312F) || (cp >= 0x31A0 && cp <= 0x31BF) ||
         (cp >= 0x31C0 && cp <= 0x31EF) || (cp >= 0x31F0 && cp <= 0x31FF) ||
         (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE4F) ||
         (cp >= 0xFF00 && cp <= 0xFFEF);
}

bool isCjkNoSpaceBoundary(const std::string_view leftWord, const std::string_view rightWord) {
  return isCjkCodepointForSpacing(lastCodepoint(leftWord)) || isCjkCodepointForSpacing(firstCodepoint(rightWord));
}

int naturalBoundaryGap(const GfxRenderer& renderer, const int fontId, const std::string_view leftWord,
                       const std::string_view rightWord, const EpdFontFamily::Style style, const bool attaches) {
  const uint32_t leftCp = lastCodepoint(leftWord);
  const uint32_t rightCp = firstCodepoint(rightWord);
  if (attaches) {
    return renderer.getKerning(fontId, leftCp, rightCp, style);
  }
  if (isCjkCodepointForSpacing(leftCp) || isCjkCodepointForSpacing(rightCp)) {
    return 0;
  }
  return renderer.getSpaceAdvance(fontId, leftCp, rightCp, style);
}

}  // namespace

void ParsedText::pushWord(const std::string_view word, const EpdFontFamily::Style style, const bool continues) {
  const auto offset = static_cast<uint32_t>(wordArena.size());
  wordArena.insert(wordArena.end(), word.begin(), word.end());
  wordOffsets.push_back(offset);
  wordLengths.push_back(static_cast<uint16_t>(word.size()));
  wordStyles.push_back(style);
  wordContinues.push_back(continues);
}

void ParsedText::eraseFront(const size_t count) {
  if (count == 0) return;
  if (count >= wordOffsets.size()) {
    wordArena.clear();
    wordOffsets.clear();
    wordLengths.clear();
    wordStyles.clear();
    wordContinues.clear();
    return;
  }
  size_t writeOffset = 0;
  char* arena = wordArena.data();
  for (size_t i = count; i < wordOffsets.size(); ++i) {
    const uint16_t length = wordLengths[i];
    if (length > 0) {
      std::memmove(arena + writeOffset, arena + wordOffsets[i], length);
    }
    wordOffsets[i] = static_cast<uint32_t>(writeOffset);
    writeOffset += length;
  }
  wordArena.resize(writeOffset);
  wordOffsets.erase(wordOffsets.begin(), wordOffsets.begin() + count);
  wordLengths.erase(wordLengths.begin(), wordLengths.begin() + count);
  wordStyles.erase(wordStyles.begin(), wordStyles.begin() + count);
  wordContinues.erase(wordContinues.begin(), wordContinues.begin() + count);
}

void ParsedText::addWord(const std::string_view word, const EpdFontFamily::Style fontStyle, const bool underline,
                         const bool attachToPrevious) {
  if (word.empty()) return;

  EpdFontFamily::Style combinedStyle = fontStyle;
  if (underline) {
    combinedStyle = static_cast<EpdFontFamily::Style>(combinedStyle | EpdFontFamily::UNDERLINE);
  }
  pushWord(word, combinedStyle, attachToPrevious);
}

// Consumes data to minimize memory usage
void ParsedText::layoutAndExtractLines(const GfxRenderer& renderer, const int fontId, const uint16_t viewportWidth,
                                       const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                                       const bool includeLastLine) {
  if (wordOffsets.empty()) {
    return;
  }

  // Apply fixed transforms before any per-line layout work.
  applyParagraphIndent();

  const int pageWidth = viewportWidth;
  auto wordWidths = calculateWordWidths(renderer, fontId);

  std::vector<size_t> lineBreakIndices;
  if (hyphenationEnabled) {
    // Use greedy layout that can split words mid-loop when a hyphenated prefix fits.
    lineBreakIndices = computeHyphenatedLineBreaks(renderer, fontId, pageWidth, wordWidths, wordContinues);
  } else {
    lineBreakIndices = computeLineBreaks(renderer, fontId, pageWidth, wordWidths, wordContinues);
  }
  if (lineBreakIndices.empty()) {
    return;
  }
  const size_t lineCount = includeLastLine ? lineBreakIndices.size() : lineBreakIndices.size() - 1;

  for (size_t i = 0; i < lineCount; ++i) {
    extractLine(i, pageWidth, wordWidths, wordContinues, lineBreakIndices, processLine, renderer, fontId);
  }

  // Remove consumed words so size() reflects only remaining words
  if (lineCount > 0) {
    const size_t consumed = lineBreakIndices[lineCount - 1];
    eraseFront(consumed);
  }
}

std::vector<uint16_t> ParsedText::calculateWordWidths(const GfxRenderer& renderer, const int fontId) {
  std::vector<uint16_t> wordWidths;
  wordWidths.reserve(wordOffsets.size());

  for (size_t i = 0; i < wordOffsets.size(); ++i) {
    wordWidths.push_back(measureWordWidth(renderer, fontId, wordView(i), wordStyles[i]));
  }

  return wordWidths;
}

std::vector<size_t> ParsedText::computeLineBreaks(const GfxRenderer& renderer, const int fontId, const int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec) {
  if (wordOffsets.empty()) {
    return {};
  }

  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const int firstLineIndent =
      blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  // Ensure any word that would overflow even as the first entry on a line is split using fallback hyphenation.
  for (size_t i = 0; i < wordWidths.size(); ++i) {
    // First word needs to fit in reduced width if there's an indent
    const int effectiveWidth = i == 0 ? pageWidth - firstLineIndent : pageWidth;
    while (wordWidths[i] > effectiveWidth) {
      if (!hyphenateWordAtIndex(i, effectiveWidth, renderer, fontId, wordWidths, /*allowFallbackBreaks=*/true)) {
        break;
      }
    }
  }

  const size_t totalWordCount = wordOffsets.size();

  // DP table to store the minimum badness (cost) of lines starting at index i
  std::vector<int> dp(totalWordCount);
  // 'ans[i]' stores the index 'j' of the *last word* in the optimal line starting at 'i'
  std::vector<size_t> ans(totalWordCount);

  // Base Case
  dp[totalWordCount - 1] = 0;
  ans[totalWordCount - 1] = totalWordCount - 1;

  for (int i = totalWordCount - 2; i >= 0; --i) {
    int currlen = 0;
    dp[i] = MAX_COST;

    // First line has reduced width due to text-indent
    const int effectivePageWidth = i == 0 ? pageWidth - firstLineIndent : pageWidth;

    for (size_t j = i; j < totalWordCount; ++j) {
      // Add space before word j, unless it's the first word on the line or a continuation
      int gap = 0;
      if (j > static_cast<size_t>(i) && !continuesVec[j]) {
        gap = naturalBoundaryGap(renderer, fontId, wordView(j - 1), wordView(j), wordStyles[j - 1], false);
      } else if (j > static_cast<size_t>(i) && continuesVec[j]) {
        // Cross-boundary kerning for continuation words (e.g. nonbreaking spaces, attached punctuation)
        gap = naturalBoundaryGap(renderer, fontId, wordView(j - 1), wordView(j), wordStyles[j - 1], true);
      }
      currlen += wordWidths[j] + gap;

      if (currlen > effectivePageWidth) {
        break;
      }

      // Cannot break after word j if the next word attaches to it (continuation group)
      if (j + 1 < totalWordCount && continuesVec[j + 1]) {
        continue;
      }

      int cost;
      if (j == totalWordCount - 1) {
        cost = 0;  // Last line
      } else {
        const int remainingSpace = effectivePageWidth - currlen;
        // Use long long for the square to prevent overflow
        const long long cost_ll = static_cast<long long>(remainingSpace) * remainingSpace + dp[j + 1];

        if (cost_ll > MAX_COST) {
          cost = MAX_COST;
        } else {
          cost = static_cast<int>(cost_ll);
        }
      }

      if (cost < dp[i]) {
        dp[i] = cost;
        ans[i] = j;  // j is the index of the last word in this optimal line
      }
    }

    // Handle oversized word: if no valid configuration found, force single-word line
    // This prevents cascade failure where one oversized word breaks all preceding words
    if (dp[i] == MAX_COST) {
      ans[i] = i;  // Just this word on its own line
      // Inherit cost from next word to allow subsequent words to find valid configurations
      if (i + 1 < static_cast<int>(totalWordCount)) {
        dp[i] = dp[i + 1];
      } else {
        dp[i] = 0;
      }
    }
  }

  // Stores the index of the word that starts the next line (last_word_index + 1)
  std::vector<size_t> lineBreakIndices;
  size_t currentWordIndex = 0;

  while (currentWordIndex < totalWordCount) {
    size_t nextBreakIndex = ans[currentWordIndex] + 1;

    // Safety check: prevent infinite loop if nextBreakIndex doesn't advance
    if (nextBreakIndex <= currentWordIndex) {
      // Force advance by at least one word to avoid infinite loop
      nextBreakIndex = currentWordIndex + 1;
    }

    lineBreakIndices.push_back(nextBreakIndex);
    currentWordIndex = nextBreakIndex;
  }

  return lineBreakIndices;
}

void ParsedText::applyParagraphIndent() {
  if (!firstLineIndent || wordOffsets.empty()) {
    return;
  }

  if (blockStyle.textIndentDefined) {
    // CSS text-indent is explicitly set (even if 0) - don't use fallback EmSpace
    // The actual indent positioning is handled in extractLine()
    return;
  }
  if (blockStyle.alignment != CssTextAlign::Justify && blockStyle.alignment != CssTextAlign::Left) {
    return;
  }

  // No CSS text-indent defined - prepend an EmSpace to the first word as a visual indent.
  // Re-emit the modified word at the end of the arena so the original bytes can remain inert.
  const uint32_t originalOffset = wordOffsets.front();
  const uint16_t originalLength = wordLengths.front();
  const auto newOffset = static_cast<uint32_t>(wordArena.size());
  const size_t newSize = wordArena.size() + EM_SPACE_BYTES + originalLength;
  wordArena.reserve(newSize);
  wordArena.resize(newSize);
  char* arena = wordArena.data();
  std::memcpy(arena + newOffset, EM_SPACE_UTF8, EM_SPACE_BYTES);
  std::memcpy(arena + newOffset + EM_SPACE_BYTES, arena + originalOffset, originalLength);
  wordOffsets.front() = newOffset;
  wordLengths.front() = static_cast<uint16_t>(EM_SPACE_BYTES + originalLength);
}

// Builds break indices while opportunistically splitting the word that would overflow the current line.
std::vector<size_t> ParsedText::computeHyphenatedLineBreaks(const GfxRenderer& renderer, const int fontId,
                                                            const int pageWidth, std::vector<uint16_t>& wordWidths,
                                                            std::vector<bool>& continuesVec) {
  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const int firstLineIndent =
      blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  std::vector<size_t> lineBreakIndices;
  size_t currentIndex = 0;
  bool isFirstLine = true;

  while (currentIndex < wordWidths.size()) {
    const size_t lineStart = currentIndex;
    int lineWidth = 0;

    // First line has reduced width due to text-indent
    const int effectivePageWidth = isFirstLine ? pageWidth - firstLineIndent : pageWidth;

    // Consume as many words as possible for current line, splitting when prefixes fit
    while (currentIndex < wordWidths.size()) {
      const bool isFirstWord = currentIndex == lineStart;
      int spacing = 0;
      if (!isFirstWord && !continuesVec[currentIndex]) {
        spacing = naturalBoundaryGap(renderer, fontId, wordView(currentIndex - 1), wordView(currentIndex),
                                     wordStyles[currentIndex - 1], false);
      } else if (!isFirstWord && continuesVec[currentIndex]) {
        // Cross-boundary kerning for continuation words (e.g. nonbreaking spaces, attached punctuation)
        spacing = naturalBoundaryGap(renderer, fontId, wordView(currentIndex - 1), wordView(currentIndex),
                                     wordStyles[currentIndex - 1], true);
      }
      const int candidateWidth = spacing + wordWidths[currentIndex];

      // Word fits on current line
      if (lineWidth + candidateWidth <= effectivePageWidth) {
        lineWidth += candidateWidth;
        ++currentIndex;
        continue;
      }

      // Word would overflow — try to split based on hyphenation points
      const int availableWidth = effectivePageWidth - lineWidth - spacing;
      const bool allowFallbackBreaks = isFirstWord;  // Only for first word on line

      if (availableWidth > 0 &&
          hyphenateWordAtIndex(currentIndex, availableWidth, renderer, fontId, wordWidths, allowFallbackBreaks)) {
        // Prefix now fits; append it to this line and move to next line
        lineWidth += spacing + wordWidths[currentIndex];
        ++currentIndex;
        break;
      }

      // Could not split: force at least one word per line to avoid infinite loop
      if (currentIndex == lineStart) {
        lineWidth += candidateWidth;
        ++currentIndex;
      }
      break;
    }

    // Don't break before a continuation word (e.g., orphaned "?" after "question").
    // Backtrack to the start of the continuation group so the whole group moves to the next line.
    while (currentIndex > lineStart + 1 && currentIndex < wordWidths.size() && continuesVec[currentIndex]) {
      --currentIndex;
    }

    lineBreakIndices.push_back(currentIndex);
    isFirstLine = false;
  }

  return lineBreakIndices;
}

// Splits words[wordIndex] into prefix (adding a hyphen only when needed) and remainder when a legal breakpoint fits the
// available width.
bool ParsedText::hyphenateWordAtIndex(const size_t wordIndex, const int availableWidth, const GfxRenderer& renderer,
                                      const int fontId, std::vector<uint16_t>& wordWidths,
                                      const bool allowFallbackBreaks) {
  // Guard against invalid indices or zero available width before attempting to split.
  if (availableWidth <= 0 || wordIndex >= wordOffsets.size()) {
    return false;
  }

  const auto style = wordStyles[wordIndex];
  // Hyphenator currently owns a std::string API. This is one transient
  // allocation per split attempt, unlike the long-lived per-token storage above.
  const std::string word{wordView(wordIndex)};

  // Collect candidate breakpoints (byte offsets and hyphen requirements).
  auto breakInfos = Hyphenator::breakOffsets(word, allowFallbackBreaks);
  if (breakInfos.empty()) {
    return false;
  }

  size_t chosenOffset = 0;
  int chosenWidth = -1;
  bool chosenNeedsHyphen = true;

  // Iterate over each legal breakpoint and retain the widest prefix that still fits.
  for (const auto& info : breakInfos) {
    const size_t offset = info.byteOffset;
    if (offset == 0 || offset >= word.size()) {
      continue;
    }

    const bool needsHyphen = info.requiresInsertedHyphen;
    const std::string_view prefix{word.data(), offset};
    const int prefixWidth = measureWordWidth(renderer, fontId, prefix, style, needsHyphen);
    if (prefixWidth > availableWidth || prefixWidth <= chosenWidth) {
      continue;  // Skip if too wide or not an improvement
    }

    chosenWidth = prefixWidth;
    chosenOffset = offset;
    chosenNeedsHyphen = needsHyphen;
  }

  if (chosenWidth < 0) {
    // No hyphenation point produced a prefix that fits in the remaining space.
    return false;
  }

  // Re-emit the prefix and remainder at the end of the arena. The old bytes are
  // left inert until this ParsedText block is destroyed, avoiding in-place string
  // growth and per-word heap churn.
  const size_t prefixLen = chosenOffset;
  const size_t hyphenBytes = chosenNeedsHyphen ? 1 : 0;
  const size_t remainderLen = word.size() - chosenOffset;
  const size_t arenaOldSize = wordArena.size();
  wordArena.resize(arenaOldSize + prefixLen + hyphenBytes + remainderLen);
  char* arena = wordArena.data();
  std::memcpy(arena + arenaOldSize, word.data(), prefixLen);
  if (chosenNeedsHyphen) {
    arena[arenaOldSize + prefixLen] = '-';
  }
  std::memcpy(arena + arenaOldSize + prefixLen + hyphenBytes, word.data() + chosenOffset, remainderLen);

  wordOffsets[wordIndex] = static_cast<uint32_t>(arenaOldSize);
  wordLengths[wordIndex] = static_cast<uint16_t>(prefixLen + hyphenBytes);
  wordOffsets.insert(wordOffsets.begin() + wordIndex + 1, static_cast<uint32_t>(arenaOldSize + prefixLen + hyphenBytes));
  wordLengths.insert(wordLengths.begin() + wordIndex + 1, static_cast<uint16_t>(remainderLen));
  wordStyles.insert(wordStyles.begin() + wordIndex + 1, style);

  // Continuation flag handling after splitting a word into prefix + remainder.
  //
  // The prefix keeps the original word's continuation flag so that no-break-space groups
  // stay linked. The remainder always gets continues=false because it starts on the next
  // line and is not attached to the prefix.
  //
  // Example: "200&#xA0;Quadratkilometer" produces tokens:
  //   [0] "200"               continues=false
  //   [1] " "                 continues=true
  //   [2] "Quadratkilometer"  continues=true   <-- the word being split
  //
  // After splitting "Quadratkilometer" at "Quadrat-" / "kilometer":
  //   [0] "200"         continues=false
  //   [1] " "           continues=true
  //   [2] "Quadrat-"    continues=true   (KEPT — still attached to the no-break group)
  //   [3] "kilometer"   continues=false  (NEW — starts fresh on the next line)
  //
  // This lets the backtracking loop keep the entire prefix group ("200 Quadrat-") on one
  // line, while "kilometer" moves to the next line.
  // wordContinues[wordIndex] is intentionally left unchanged — the prefix keeps its original attachment.
  wordContinues.insert(wordContinues.begin() + wordIndex + 1, false);

  // Update cached widths to reflect the new prefix/remainder pairing.
  wordWidths[wordIndex] = static_cast<uint16_t>(chosenWidth);
  const std::string_view remainderView{word.data() + chosenOffset, remainderLen};
  const uint16_t remainderWidth = measureWordWidth(renderer, fontId, remainderView, style);
  wordWidths.insert(wordWidths.begin() + wordIndex + 1, remainderWidth);
  return true;
}

void ParsedText::extractLine(const size_t breakIndex, const int pageWidth, const std::vector<uint16_t>& wordWidths,
                             const std::vector<bool>& continuesVec, const std::vector<size_t>& lineBreakIndices,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             const GfxRenderer& renderer, const int fontId) {
  const size_t lineBreak = lineBreakIndices[breakIndex];
  const size_t lastBreakAt = breakIndex > 0 ? lineBreakIndices[breakIndex - 1] : 0;
  const size_t lineWordCount = lineBreak - lastBreakAt;

  // Calculate first line indent (only for left/justified text).
  // Positive text-indent (paragraph indent) is suppressed when extraParagraphSpacing is on.
  // Negative text-indent (hanging indent, e.g. margin-left:3em; text-indent:-1em) always applies —
  // it is structural (positions the bullet/marker), not decorative.
  const bool isFirstLine = breakIndex == 0;
  const int firstLineIndent =
      isFirstLine && blockStyle.textIndentDefined && (blockStyle.textIndent < 0 || !extraParagraphSpacing) &&
              (blockStyle.alignment == CssTextAlign::Justify || blockStyle.alignment == CssTextAlign::Left)
          ? blockStyle.textIndent
          : 0;

  // Calculate total word width for this line, count actual word gaps,
  // and accumulate total natural gap widths (including space kerning adjustments).
  int lineWordWidthSum = 0;
  size_t actualGapCount = 0;
  int totalNaturalGaps = 0;

  for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
    lineWordWidthSum += wordWidths[lastBreakAt + wordIdx];
    // Count gaps: each word after the first creates a gap, unless it's a continuation
    if (wordIdx > 0) {
      const size_t currentWord = lastBreakAt + wordIdx;
      const bool attaches = continuesVec[currentWord];
      if (!attaches && !isCjkNoSpaceBoundary(wordView(currentWord - 1), wordView(currentWord))) {
        actualGapCount++;
      }
      // Cross-boundary kerning for continuation words (e.g. nonbreaking spaces, attached punctuation)
      totalNaturalGaps += naturalBoundaryGap(renderer, fontId, wordView(currentWord - 1), wordView(currentWord),
                                             wordStyles[currentWord - 1], attaches);
    }
  }

  // Calculate spacing (account for indent reducing effective page width on first line)
  const int effectivePageWidth = pageWidth - firstLineIndent;
  const bool isLastLine = breakIndex == lineBreakIndices.size() - 1;

  // For justified text, compute per-gap extra to distribute remaining space evenly
  const int spareSpace = effectivePageWidth - lineWordWidthSum - totalNaturalGaps;
  const int justifyExtra = (blockStyle.alignment == CssTextAlign::Justify && !isLastLine && actualGapCount >= 1)
                               ? spareSpace / static_cast<int>(actualGapCount)
                               : 0;

  // Calculate initial x position (first line starts at indent for left/justified text;
  // may be negative for hanging indents, e.g. margin-left:3em; text-indent:-1em).
  auto xpos = static_cast<int16_t>(firstLineIndent);
  if (blockStyle.alignment == CssTextAlign::Right) {
    xpos = effectivePageWidth - lineWordWidthSum - totalNaturalGaps;
  } else if (blockStyle.alignment == CssTextAlign::Center) {
    xpos = (effectivePageWidth - lineWordWidthSum - totalNaturalGaps) / 2;
  }

  // Pre-calculate X positions for words
  // Continuation words attach to the previous word with no space before them
  std::vector<int16_t> lineXPos;
  lineXPos.reserve(lineWordCount);

  for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
    lineXPos.push_back(xpos);

    const bool nextIsContinuation = wordIdx + 1 < lineWordCount && continuesVec[lastBreakAt + wordIdx + 1];
    if (nextIsContinuation) {
      int advance = wordWidths[lastBreakAt + wordIdx];
      // Cross-boundary kerning for continuation words (e.g. nonbreaking spaces, attached punctuation)
      advance += naturalBoundaryGap(renderer, fontId, wordView(lastBreakAt + wordIdx),
                                    wordView(lastBreakAt + wordIdx + 1), wordStyles[lastBreakAt + wordIdx], true);
      xpos += advance;
    } else {
      int gap = 0;
      if (wordIdx + 1 < lineWordCount) {
        gap = naturalBoundaryGap(renderer, fontId, wordView(lastBreakAt + wordIdx),
                                 wordView(lastBreakAt + wordIdx + 1), wordStyles[lastBreakAt + wordIdx], false);
      }
      if (blockStyle.alignment == CssTextAlign::Justify && !isLastLine && wordIdx + 1 < lineWordCount &&
          !isCjkNoSpaceBoundary(wordView(lastBreakAt + wordIdx), wordView(lastBreakAt + wordIdx + 1))) {
        gap += justifyExtra;
      }
      xpos += wordWidths[lastBreakAt + wordIdx] + gap;
    }
  }

  std::vector<std::string> lineWords;
  lineWords.reserve(lineWordCount);
  for (size_t wordIdx = 0; wordIdx < lineWordCount; ++wordIdx) {
    const std::string_view view = wordView(lastBreakAt + wordIdx);
    lineWords.emplace_back(view.data(), view.size());
  }
  std::vector<EpdFontFamily::Style> lineWordStyles(wordStyles.begin() + lastBreakAt, wordStyles.begin() + lineBreak);

  for (auto& word : lineWords) {
    if (word.find(SOFT_HYPHEN_UTF8, 0, SOFT_HYPHEN_BYTES) != std::string::npos) {
      stripSoftHyphensInPlace(word);
    }
  }

  processLine(
      std::make_shared<TextBlock>(std::move(lineWords), std::move(lineXPos), std::move(lineWordStyles), blockStyle));
}
