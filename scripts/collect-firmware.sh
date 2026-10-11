#!/bin/sh
# After `esphome compile <config>`, copies the config's firmware images to
# <outdir> and writes an ESP Web Tools manifest for the web installer:
#   <name>.factory.bin   full image for a blank chip (web installer)
#   <name>.ota.bin       update image for OTA
#   <name>.manifest.json
#
#   scripts/collect-firmware.sh firmware/esp32-s3.yaml dist 1.2.0
set -eu
config=$1
out=$2
version=$3

storage="$(dirname "$config")/.esphome/storage/$(basename "$config").json"
[ -f "$storage" ] || { echo "no build info at $storage; compile first" >&2; exit 1; }

name=$(jq -r .name "$storage")
friendly=$(jq -r '.friendly_name // .name' "$storage")
build=$(jq -r .build_path "$storage")
# ESP32S3 -> ESP32-S3, the chip family names ESP Web Tools uses.
chip=$(jq -r .esp_platform "$storage" | sed -E 's/^ESP32(.+)$/ESP32-\1/')

# Native ESP-IDF builds and PlatformIO builds put images in different places.
factory=$(find "$build" -name firmware.factory.bin | head -n 1)
ota=$(find "$build" -name firmware.ota.bin | head -n 1)
[ -n "$factory" ] && [ -n "$ota" ] || { echo "no firmware images under $build" >&2; exit 1; }

mkdir -p "$out"
cp "$factory" "$out/$name.factory.bin"
cp "$ota" "$out/$name.ota.bin"
jq -n --arg name "$friendly" --arg version "$version" --arg chip "$chip" --arg bin "$name.factory.bin" '{
  name: $name,
  version: $version,
  new_install_prompt_erase: true,
  builds: [{chipFamily: $chip, parts: [{path: $bin, offset: 0}]}]
}' > "$out/$name.manifest.json"
echo "$config -> $out/$name.{factory.bin,ota.bin,manifest.json} ($chip)"
