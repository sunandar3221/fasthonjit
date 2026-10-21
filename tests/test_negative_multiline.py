import subprocess
import os

tests = [
    ('multi-line fn without end', """kali = fn(x, y)
    return x * y
"""),
    ('multi-line if without end', """x = 10
s = if x > 5
    "besar"
else
    "kecil"
"""),
    ('multi-line loop without end', """l = loop 3
    "A"
""")
]

for name, code in tests:
    filename = "temp_fail_test.fsn"
    with open(filename, "w", encoding="utf-8") as f:
        f.write(code)
    p = subprocess.run([".\\fasthonjit.exe", filename], capture_output=True, text=True)
    err = p.stderr or p.stdout
    print(f"=== Test: {name} ===")
    print(f"ReturnCode: {p.returncode}")
    print(err)
    assert p.returncode != 0, f"{name} should have failed!"
    assert "Diharapkan 'end'" in err, f"Error message should mention 'end'!"
    print("PASS: correctly rejected missing end!\n")

if os.path.exists("temp_fail_test.fsn"):
    os.remove("temp_fail_test.fsn")

print("SEMUA PENGUJIAN NEGATIF (WAJIB END MULTI-LINE) BERHASIL 100%!")
