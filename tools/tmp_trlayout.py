import io
p='drivers/keyboard.c'; s=io.open(p,encoding='utf-8').read()

# Turkish Q: row1 ends with ğ ü, row2 has ş ı, row3 has ö ç
s=s.replace('''    L("tr-q", "Turkish (Q)",
      "1234567890*-", "!'^+%&/()=?`",
      "qwertyuiopgu", "QWERTYUIOPGU",
      "asdfghjklsi",  "ASDFGHJKLSI",
      "zxcvbnmoc.",   "ZXCVBNMOC:",
      "<>",           "><"),''',
'''    L("tr-q", "Turkish (Q)",
      "1234567890*-", "!'^+%&/()=?\\x8B",
      "qwertyuiop\\x80\\x81", "QWERTYUIOP\\x8B\\x87",
      "asdfghjkl\\x82\\x83",  "ASDFGHJKL\\x88\\x86",
      "zxcvbnm\\x84\\x85.",   "ZXCVBNM\\x89\\x8A:",
      "<>",           "><"),''')

# Turkish F: similar layout
s=s.replace('''    L("tr-f", "Turkish (F)",
      "1234567890*-", "!'^+%&/()=?`",
      "fggiodrnhpqw", "FGGIODRNHPQW",
      "uieautkmlys",  "UIEAUTKMLYS",
      "cxvbnm,.-/",   "CXVBNM;:_?",
      "<>",           "><"),''',
'''    L("tr-f", "Turkish (F)",
      "1234567890*-", "!'^+%&/()=?\\x8B",
      "f\\x80\\x80iodrnhpqw", "F\\x8B\\x8BIODRNHPQW",
      "uieautkmly\\x82",  "UIEAUTKMLY\\x88",
      "cxvbnm\\x84\\x85-/",   "CXVBNM\\x89\\x8A:_?",
      "<>",           "><"),''')
io.open(p,'w',encoding='utf-8').write(s)
print('ok')
