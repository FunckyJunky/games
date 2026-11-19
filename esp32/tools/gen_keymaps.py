# makes the keyboard layout tables in keymaps.h (HID usage 0x04..0x38, 0x64 -> character)
# order of keys in each row string: usages 0x04..0x38 then 0x64
US  = ("abcdefghijklmnopqrstuvwxyz1234567890" + "\n\x1b\b\t " + "-=[]\\#;'`,./" + "\\",
       "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()" + "\n\x1b\b\t " + "_+{}|~:\"~<>?" + "|")
DE  = ("abcdefghijklmnopqrstuvwxzy1234567890" + "\n\x1b\b\t " + "ß´ü+##öä^,.-" + "<",
       "ABCDEFGHIJKLMNOPQRSTUVWXZY!\"§$%&/()=" + "\n\x1b\b\t " + "?`Ü*''ÖÄ°;:_" + ">")
FR  = ("qbcdefghijkl,noparstuvzxyw&é\"'(-è_çà" + "\n\x1b\b\t " + ")=^$**mù²;:!" + "<",
       "QBCDEFGHIJKL?NOPARSTUVZXYW1234567890" + "\n\x1b\b\t " + "°+¨£µµM%~./§" + ">")
UA  = ("фисвуапршолдьтщзйкіегмцчня1234567890" + "\n\x1b\b\t " + "-=хї\\\\жє'бю." + "ґ",
       "ФИСВУАПРШОЛДЬТЩЗЙКІЕГМЦЧНЯ!\"№;%:?*()" + "\n\x1b\b\t " + "_+ХЇ//ЖЄ'БЮ," + "Ґ")
# AltGr (right Alt) extras: (layout, usage, char)
ALTGR = [("DE",0x14,"@"),("DE",0x08,"€"),("DE",0x24,"{"),("DE",0x25,"["),("DE",0x26,"]"),("DE",0x27,"}"),("DE",0x2D,"\\"),("DE",0x30,"~"),("DE",0x64,"|"),
         ("FR",0x27,"@"),("FR",0x20,"#"),("FR",0x21,"{"),("FR",0x22,"["),("FR",0x23,"|"),("FR",0x25,"\\"),("FR",0x26,"^"),("FR",0x2D,"]"),("FR",0x2E,"}"),("FR",0x08,"€"),
         ("UA",0x18,"ґ")]
usages = list(range(0x04, 0x39)) + [0x64]
out = ["#pragma once", "// made by esp32/tools/gen_keymaps.py: HID usage 0x04..0x38 and 0x64 -> Unicode (0 = not a character)",
       "static const uint8_t KEYMAP_KEYS = %d;" % len(usages)]
for name, (lo, hi) in (("US", US), ("DE", DE), ("FR", FR), ("UA", UA)):
    assert len(lo) == len(usages) and len(hi) == len(usages), (name, len(lo), len(hi), len(usages))
    for tag, row in (("", lo), ("_SHIFT", hi)):
        vals = [0 if c in "\n\x1b\b\t" else ord(c) for c in row]
        out.append("static const uint16_t KM_%s%s[%d] = {%s};" % (name, tag, len(vals), ",".join("0x%X" % v for v in vals)))
out.append("struct AltGrKey { uint8_t layout; uint8_t usage; uint16_t ch; };")
L = {"US": 0, "DE": 1, "FR": 2, "UA": 3}
out.append("static const AltGrKey ALTGR[] = {%s};" % ",".join("{%d,0x%X,0x%X}" % (L[l], u, ord(c)) for l, u, c in ALTGR))
open(__import__("os").path.join(__import__("os").path.dirname(__file__), "..", "ErrorOled", "keymaps.h"), "w").write("\n".join(out) + "\n")
print("ok", len(usages))
