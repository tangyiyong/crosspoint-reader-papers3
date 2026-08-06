#pragma once

#include <EpdFontFamily.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "blocks/BlockStyle.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  std::vector<char> wordArena;
  std::vector<uint32_t> wordOffsets;
  std::vector<uint16_t> wordLengths;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;  // true = word attaches to previous (no space before it)
  BlockStyle blockStyle;
  bool extraParagraphSpacing;
  bool firstLineIndent;
  bool hyphenationEnabled;

  std::string_view wordView(size_t i) const { return {wordArena.data() + wordOffsets[i], wordLengths[i]}; }
  void pushWord(std::string_view word, EpdFontFamily::Style style, bool continues);
  void eraseFront(size_t count);
  void applyParagraphIndent();
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                        std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks);
  void extractLine(size_t breakIndex, int pageWidth, const std::vector<uint16_t>& wordWidths,
                   const std::vector<bool>& continuesVec, const std::vector<size_t>& lineBreakIndices,
                   const std::function<void(std::shared_ptr<TextBlock>)>& processLine, const GfxRenderer& renderer,
                   int fontId);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);

 public:
  explicit ParsedText(const bool extraParagraphSpacing, const bool hyphenationEnabled = false,
                      const BlockStyle& blockStyle = BlockStyle(), const bool firstLineIndent = false)
      : blockStyle(blockStyle),
        extraParagraphSpacing(extraParagraphSpacing),
        firstLineIndent(firstLineIndent),
        hyphenationEnabled(hyphenationEnabled) {}
  ~ParsedText() = default;

  void addWord(std::string_view word, EpdFontFamily::Style fontStyle, bool underline = false,
               bool attachToPrevious = false);
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  BlockStyle& getBlockStyle() { return blockStyle; }
  size_t size() const { return wordOffsets.size(); }
  bool isEmpty() const { return wordOffsets.empty(); }
  void layoutAndExtractLines(const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             bool includeLastLine = true);
};
