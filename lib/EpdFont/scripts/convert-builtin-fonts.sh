#!/bin/bash

set -e

cd "$(dirname "$0")"

READER_FONT_STYLES=("Regular" "Italic" "Bold" "BoldItalic")
BOOKERLY_FONT_SIZES=(12 14 16 18)
NOTOSANS_FONT_SIZES=(12 14 16 18)
OPENDYSLEXIC_FONT_SIZES=(8 10 12 14)
NOTOSANSSC_FONT_SIZES=(10 14)

for size in ${BOOKERLY_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="bookerly_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/Bookerly/Bookerly-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress > $output_path
    echo "Generated $output_path"
  done
done

for size in ${NOTOSANS_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="notosans_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/NotoSans/NotoSans-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress > $output_path
    echo "Generated $output_path"
  done
done

for size in ${OPENDYSLEXIC_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="opendyslexic_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/OpenDyslexic/OpenDyslexic-${style}.otf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress > $output_path
    echo "Generated $output_path"
  done
done

for size in ${NOTOSANSSC_FONT_SIZES[@]}; do
  font_name="notosanssc_${size}_regular"
  notosanssc_font_stack=(../builtinFonts/source/NotoSansSC/400/*.woff)
  latin_fallback_path="../builtinFonts/source/NotoSans/NotoSans-Regular.ttf"
  output_path="../builtinFonts/${font_name}.h"
  python fontconvert.py "$font_name" "$size" "${notosanssc_font_stack[@]}" "$latin_fallback_path" --2bit --compress \
    --force-autohint --no-kerning --no-ligatures --additional-intervals 0x02C0,0x02FF \
    --additional-intervals 0x0370,0x03FF --additional-intervals 0x0400,0x04FF \
    --additional-intervals 0x0600,0x06FF --additional-intervals 0x0E00,0x0E7F \
    --additional-intervals 0x2000,0x206F --additional-intervals 0x2190,0x21FF \
    --additional-intervals 0x2300,0x23FF --additional-intervals 0x2460,0x24FF \
    --additional-intervals 0x2500,0x257F --additional-intervals 0x25A0,0x25FF \
    --additional-intervals 0x2600,0x26FF --additional-intervals 0x2700,0x27BF \
    --additional-intervals 0x2E80,0x303F --additional-intervals 0x3040,0x309F \
    --additional-intervals 0x30A0,0x30FF --additional-intervals 0x4E00,0x9FFF \
    --additional-intervals 0xFE30,0xFE4F --additional-intervals 0xFF00,0xFFEF > "$output_path"
  echo "Generated $output_path"
done

UI_FONT_SIZES=(10 12)
UI_FONT_STYLES=("Regular" "Bold")

for size in ${UI_FONT_SIZES[@]}; do
  for style in ${UI_FONT_STYLES[@]}; do
    font_name="ubuntu_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/Ubuntu/Ubuntu-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path > $output_path
    echo "Generated $output_path"
  done
done

python fontconvert.py notosans_8_regular 8 ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf > ../builtinFonts/notosans_8_regular.h

echo ""
echo "Running compression verification..."
python verify_compression.py ../builtinFonts/
