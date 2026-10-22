# FasthonJIT (.fsn)

FasthonJIT adalah bahasa pemrograman modern berkecepatan ultra-tinggi yang didukung mesin Just-In-Time (JIT) Compiler native untuk arsitektur **x86_64** dan **ARM64 (AArch64)**, dukungan platform **ARM32 (armv7l)** dalam mode VM berkinerja tinggi, filosofi sintaks ultra-bersih (ultra-clean syntax), manajemen memori deterministik tanpa Garbage Collector (Zero-Cost RAII), arsitektur multi-tier engine (Ultra Native JIT Compiler, In-Memory VM & AOT Compiler), serta standard library bawaan yang lengkap (*batteries-included*).

Proyek ini dilisensikan di bawah **Mozilla Public License 2.0 (MPL-2.0)**.

---

## Daftar Isi
1. [Filosofi & Arsitektur](#1-filosofi--arsitektur)
2. [Tutorial & Panduan Instalasi](#2-tutorial--panduan-instalasi)
3. [Kursus Kilat Fasthon (Crash Course)](#3-kursus-kilat-fasthon-crash-course)
   - [Bab 13: Tutorial Kilat & Panduan Lengkap Seluruh Modul Bawaan](#bab-13-tutorial-kilat--panduan-lengkap-seluruh-modul-bawaan-standard-library-deep-dive)
4. [Kamus Keyword & Operator](#4-kamus-keyword--operator)
5. [Sistem Diagnostik Cerdas & Rekomendasi Typo](#5-sistem-diagnostik-cerdas--rekomendasi-typo)
6. [Hasil Benchmark & Komparasi Kecepatan](#6-hasil-benchmark--komparasi-kecepatan)
7. [Panduan Eksekusi & Kompilasi AOT (Linux, Android Termux, Windows)](#7-panduan-eksekusi--kompilasi-aot-linux-android-termux-windows)
8. [Lisensi](#8-lisensi)

---

## 1. Filosofi & Arsitektur

- **Ultra-Clean Syntax**:
  - Bebas dari kurung kurawal `{}`
  - Bebas dari titik dua `:`
  - Bebas dari titik koma `;`
  - Bebas dari keyword `return` (ekspresi terakhir yang dievaluasi otomatis menjadi nilai balik fungsi)
  - Tanpa deklarasi tipe data wajib yang bertele-tele
  - Blok kode berbasis baris dan indentasi alami (dengan opsi penutup blok `end`)
- **Arsitektur Multi-Tier Execution & Native JIT Engine**:
  - **Ultra-Fast In-Memory JIT Compiler (x86_64 & ARM64)**: Mengompilasi bytecode secara real-time langsung ke instruksi mesin native murni (x86_64 atau AArch64) di dalam executable memory (`PAGE_EXECUTE_READWRITE` / `mmap PROT_EXEC`). Komputasi intensif, perulangan, dan pemanggilan rekursif dieksekusi seketika pada kecepatan *bare-metal machine code* tanpa interpretasi virtual machine.
  - **In-Memory Flat-Stack VM Fallback & ARM32 Support**: Menyediakan eksekusi instan (0 ms compile time) untuk dynamic constructs tanpa membuat berkas cache di disk (*zero storage bloat*). Pada arsitektur 32-bit seperti **ARM32 (armv7l / armeabi-v7a)**, FasthonJIT berjalan mulus dalam mode VM interpreter teroptimasi tinggi tanpa overhead alokasi GC.
  - **Ahead-Of-Time (AOT) Compiler Backend (`fasthonjit build`)**: Mengompilasi skrip `.fsn` langsung menjadi berkas binary native mandiri (`.exe` di Windows, ELF di Linux/Android) menggunakan optimasi agresif `-O3 -march=native` tanpa dependensi runtime.
- **Manajemen Memori Deterministik (Zero GC Overhead)**:
  - Menggunakan model Zero-Cost RAII sehingga alokasi dan dealokasi memori terjadi seketika tanpa jeda *stop-the-world garbage collection*.
- **Batteries-Included Standard Library**:
  - File I/O bawaan berkecepatan tinggi (`read`, `write`, stream `io.open`)
  - Sistem Operasi & Manajemen Filesystem (`os`, `os.path` / `path`)
  - Akses Informasi Sistem & Argumen CLI (`sys`, `sys.argv`, `sys.platform`)
  - Serialisasi & Parsing Format Modern (`json.dumps`, `json.loads`)
  - Matematika Lengkap, Trigonometri & PRNG Hardware (`math`)
  - Waktu Presisi Tinggi, Stopwatch & Formatting (`time`)
  - Manipulasi String Tingkat Lanjut (`str.split`, `str.join`, `str.replace`, `str.trim`)
  - Jaringan dan HTTP Client (`get`, `send`, `net`)
  - Objek, map, dan list dinamis dengan perbandingan *deep-equality*
  - Scaffolding aplikasi desktop native Win32 (`app`, `window`, `run`)

---

## 2. Tutorial & Panduan Instalasi

Seluruh berkas binary resmi FasthonJIT otomatis dikompilasi oleh **GitHub Actions CI/CD** untuk multi-platform (**Linux x86_64**, **Linux ARM64**, **Linux ARM32**, **Android Termux ARM64**, **Android Termux ARM32**, dan **Windows x64**). Anda **tidak perlu repot mengompilasi manual**.

---

### 🐧 Tutorial Instalasi di Linux (Ubuntu / Debian / Arch / Fedora / WSL)

1. **Buka Terminal** Anda.
2. **Jalankan Installer 1-Baris**:
   ```bash
   curl -sSL https://raw.githubusercontent.com/sunandar3221/fasthonjit/main/install.sh | bash
   ```
   > 💡 **Pilihan Versi Interaktif & Otomatis**:
   > - **Mode Interaktif**: Jika dijalankan langsung di terminal, installer akan menampilkan menu pilihan:
   >   - Ketik `3` untuk **FasthonJIT v3.0.0** (Prerelease terbaru; pilihan default).
   >   - Ketik `2` untuk **FasthonJIT v2.0.0**.
   >   - Ketik `1` untuk **Fasthon v1.0.0** (Versi lama).
   > - **Otomatis Tanpa Tanya (Non-Interaktif)**:
   >   - Pasang Versi 3: `curl -sSL https://raw.githubusercontent.com/sunandar3221/fasthonjit/main/install.sh | bash -s -- 3`
   >   - Pilih versi 2 atau 1 dengan mengganti argumen terakhir menjadi `2` atau `1`.
   > - **Fitur Installer**:
   >   - Mendeteksi arsitektur CPU secara otomatis (`x86_64`, `ARM64/aarch64`, atau `ARM32/armv7`).
   >   - Mengunduh binary prebuilt teroptimasi dari GitHub Releases.
   >   - Memasang binary langsung ke `/usr/local/bin/fasthonjit` (serta symlink alias `fasthon` dan `eas`).

3. **Verifikasi Instalasi**:
   Ketik perintah `fasthonjit` untuk masuk ke interactive REPL:
   ```bash
   fasthonjit
   ```
   Atau cek info status mesin JIT:
   ```bash
   fasthonjit --jit-info
   ```
   Atau buat dan jalankan skrip pertama Anda:
   ```bash
   echo 'print "Halo dari FasthonJIT di Linux!"' > halo.fsn
   fasthonjit halo.fsn
   ```

---

### 📱 Tutorial Instalasi di Android (Termux)

FasthonJIT dapat berjalan secara native dan berkecepatan penuh di smartphone Android Anda menggunakan aplikasi **Termux** (mendukung perangkat ARM64 maupun ARM32):

1. **Buka Aplikasi Termux** di Android Anda.
2. **Siapkan Paket Pendukung**:
   ```bash
   pkg update && pkg install curl -y
   ```
3. **Jalankan Installer 1-Baris**:
   ```bash
   curl -sSL https://raw.githubusercontent.com/sunandar3221/fasthonjit/main/install.sh | bash
   ```
   > 💡 **Kelebihan di Termux:**
   > - **Dukungan Arsitektur Otomatis**: Installer otomatis mengunduh biner native Android Bionic ARM64 (`fasthonjit-android-arm64` dengan JIT Engine aktif) pada HP 64-bit, atau ARM32 (`fasthonjit-android-armv7` dalam mode VM) pada HP 32-bit.
   > - **Pilihan Versi**: Pilih **3** untuk v3.0.0 prerelease, **2** untuk v2.0.0, atau **1** untuk v1.0.0 saat installer berjalan. Non-interaktif, teruskan pilihan sebagai argumen terakhir (`3`, `2`, atau `1`).
   > - Otomatis terpasang ke `$PREFIX/bin/fasthonjit` (serta alias `fasthon` dan `eas`) sehingga tidak butuh akses root/sudo sama sekali.

4. **Verifikasi Instalasi**:
   Ketik perintah berikut di Termux:
   ```bash
   fasthonjit
   ```
5. **Coba Jalankan Skrip Pertama di Android**:
   ```bash
   echo 'nama = "Android Termux"' > coba.fsn
   echo 'print "FasthonJIT berjalan mulus di ${nama}"' >> coba.fsn
   fasthonjit coba.fsn
   ```

---

### 🪟 Tutorial Instalasi di Windows

Anda dapat memilih salah satu metode berikut:

#### Opsi 1: Installer Otomatis via PowerShell 1-Baris (Paling Praktis)
Buka PowerShell (tekan `Win + X` lalu pilih Terminal/PowerShell) dan jalankan:
```powershell
irm https://raw.githubusercontent.com/sunandar3221/fasthonjit/main/install.ps1 | iex
```
> ✨ Installer menampilkan pilihan versi **3**, **2**, atau **1** dan default ke v3.0.0 prerelease. Untuk memilih tanpa prompt, set `$env:FASTHON_VERSION = "2"` (atau `"1"`/`"3"`) sebelum menjalankan installer. Binary diunduh dari release repo `sunandar3221/fasthonjit`.

#### Opsi 2: Unduh Cepat via Command Prompt (CMD) atau PowerShell
Jalankan salah satu baris di bawah ini:
- **PowerShell**:
  ```powershell
  Invoke-WebRequest -Uri "https://github.com/sunandar3221/fasthonjit/releases/download/v3.0.0/fasthonjit.exe" -OutFile "$HOME\AppData\Local\Microsoft\WindowsApps\fasthonjit.exe"
  ```
- **Command Prompt (CMD)**:
  ```cmd
  curl.exe -sSL https://github.com/sunandar3221/fasthonjit/releases/download/v3.0.0/fasthonjit.exe -o "%USERPROFILE%\AppData\Local\Microsoft\WindowsApps\fasthonjit.exe"
  ```

#### Opsi 3: Unduh Manual dari GitHub Releases
1. Kunjungi [Halaman Rilis GitHub FasthonJIT](https://github.com/sunandar3221/fasthonjit/releases/latest).
2. Unduh berkas **`fasthonjit.exe`** (atau `fasthonjit-windows-x64.exe`).
3. Simpan berkas di folder pilihan Anda (misal `C:\FasthonJIT\fasthonjit.exe`).
4. *(Opsional)* Tambahkan folder tersebut ke `PATH` di Environment Variables Windows.
5. Buka Command Prompt (CMD) atau PowerShell, lalu jalankan:
   ```cmd
   fasthonjit.exe
   ```

---

### 🛠️ Opsi: Kompilasi Mandiri dari Source Code (Khusus Developer)

Bagi pengembang yang ingin memodifikasi atau berkontribusi pada source code FasthonJIT:

- **Linux / macOS / Termux**:
  ```bash
  git clone https://github.com/sunandar3221/fasthonjit.git
  cd fasthonjit
  make
  sudo make install   # (di Termux cukup: make install)
  ```

- **Windows (MinGW / GCC / Clang)**:
  ```bash
  git clone https://github.com/sunandar3221/fasthonjit.git
  cd fasthonjit
  g++ -std=c++20 -O3 -march=native src/*.cpp -Iinclude -lwininet -lgdi32 -luser32 -o fasthonjit.exe
  ```

---

## 3. Kursus Kilat Fasthon (Crash Course)

### Bab 1: Output, Variabel (`var`/`let`), Quotes, Semicolons & Komentar Komprehensif
Di Fasthon, Anda bebas menulis variabel secara langsung atau menggunakan kata kunci `var` / `let`. String dapat diapit kutip tunggal (`'...'`) maupun kutip ganda (`"..."`). Output ditampilkan menggunakan keyword `print`. Pemanggilan variabel di dalam string dapat ditulis langsung menggunakan **String Interpolation** (`${variabel}` atau `$variabel`). Banyak statement dalam 1 baris dapat dipisahkan dengan titik koma (`;`). Komentar didukung dalam segala gaya populer:

```fasthon
# Komentar satu baris gaya Python/Shell
// Komentar satu baris gaya C/C++/JavaScript/Java
/* Komentar blok multi-baris
   gaya C / C++ */
-- Komentar satu baris gaya Lua
--[[ Komentar blok multi-baris
     gaya Lua ]]

# Deklarasi variabel bebas (langsung atau via var / let)
nama = 'Budi'
var umur = 20
let kota = "Jakarta"

# Banyak statement dalam satu baris (dipisahkan titik koma)
x = 10; y = 20; z = x + y;

# String Interpolation (${variabel} atau $variabel)
print "Halo ${nama}, umur kamu ${umur} tinggal di ${kota}"
print "Tahun depan umur kamu: ${umur + 1}"
print("Total x + y =", z)
```

> ⚡ **Performa Maksimal (Zero Runtime Overhead)**:
> String interpolation di Fasthon diubah otomatis pada tahap parsing (*compile-time desugaring*) menjadi native concatenation. Ini membuat eksekusi string interpolation di Virtual Machine maupun biner native AOT berjalan dengan **kecepatan penuh tanpa latensi parsing di runtime**!


### Bab 2: Operasi Aritmatika & Ekspresi
Fasthon mendukung operator standar `+`, `-`, `*`, `/`, `%` dengan prioritas matematis yang benar serta pengelompokan menggunakan tanda kurung `()`:

```fasthon
val1 = 100
val2 = 25
sum = val1 + val2
diff = val1 - val2
prod = val1 * val2
quot = val1 / val2

print sum
print diff
print prod
print quot
```

### Bab 3: List / Array Dinamis (`push`, `pop`, `len`)
List dideklarasikan dengan kurung siku `[]` dan diakses menggunakan indeks berbasis nol `[i]`. Tersedia fungsi bawaan `len(list)` untuk mengetahui panjang list, `push(list, item)` untuk menambah elemen, dan `pop(list)` untuk mengambil elemen terakhir:

```fasthon
buah = ["Apel", "Jeruk"]

# Menambah elemen baru ke list
push(buah, "Mangga")

# Mengetahui jumlah elemen dalam list
print len(buah)          # Output: 3

# Mengakses elemen berdasarkan indeks
print buah[0]            # Output: Apel
print buah[2]            # Output: Mangga

# Mengambil dan menghapus elemen terakhir
terakhir = pop(buah)
print "Dihapus: " + terakhir  # Output: Mangga
print len(buah)          # Output: 2
```

### Bab 4: Percabangan (`if` - `elseif` / `elif` - `else`), If Expression, & Pencocokan Teks
Percabangan menggunakan kata kunci `if`, `elseif` (atau `elif`), dan `else`. Sesuai kaidah gaya Lua yang tahan banting, setiap blok percabangan **wajib ditutup dengan kata kunci `end`**.

#### 1. Percabangan Bersarang & Multikondisi (`elseif` / `elif`)
Anda dapat menggunakan `elseif` maupun `elif` untuk mengecek banyak kondisi secara berurutan:
```fasthon
nilai = 85

if nilai >= 90
    print "Grade A"
elseif nilai >= 80
    print "Grade B"
elif nilai >= 70
    print "Grade C"
else
    print "Grade D"
end
```

#### 2. If Expression (Menugaskan Nilai Percabangan ke Variabel)
Hasil evaluasi `if` dapat ditugaskan langsung ke sebuah variabel sebagai ekspresi yang sangat bersih (*clean ternary*):
```fasthon
nilai = 85

status = if nilai >= 75
    "Selamat Anda Lulus"
else
    "Silakan Mengulang Ujian"
end

print status  # Output: Selamat Anda Lulus
```

#### 3. Pencocokan Teks: Case Sensitive & Incase Sensitive
Fasthon menyediakan fungsi bawaan untuk membedakan maupun menyamakan perbandingan string antara huruf kapital dan non-kapital:
- **`case_sensitive(a, b)`**: Membedakan huruf kapital dan non-kapital secara ketat (menghasilkan `true` jika kedua teks persis sama, misal `"Agus"` dengan `"Agus"` adalah `true`, tapi `"Agus"` dengan `"agus"` adalah `false`).
- **`incase_sensitive(a, b)`**: Menyamakan huruf kapital dan non-kapital (*case-insensitive*, menghasilkan `true` meskipun huruf besar/kecil berbeda, misal `"Agus"` dengan `"agus"` adalah `true`).
- **`lower(teks)`**: Mengubah teks menjadi huruf kecil (*lowercase*).
- **`upper(teks)`**: Mengubah teks menjadi huruf besar kapital (*uppercase*).

```fasthon
nama = "Agus"

# Menyamakan huruf kapital dan non-kapital (Incase Sensitive)
if incase_sensitive(nama, "agus")
    print "Nama cocok (mengabaikan huruf besar/kecil)!"
end

# Membedakan huruf kapital dan non-kapital (Case Sensitive)
if case_sensitive(nama, "agus")
    print "Sama persis"
else
    print "Huruf besar/kecil berbeda: 'Agus' != 'agus'"
end

# Mengubah bentuk huruf
print lower("Halo Dunia")  # Output: halo dunia
print upper("halo dunia")  # Output: HALO DUNIA
```

### Bab 5: Perulangan Komprehensif (`for`, `loop`, `while`, `repeat ... until`), `break`, dan `continue`
Fasthon v2.0.0 menghadirkan arsitektur perulangan yang sangat fleksibel dan berkecepatan tinggi:
- **Numeric For Loop**: `for i = start, end [, step] [do] ... end` atau `for i = start to end [step s] [do] ... end`.
- **For-In Loop (Iterasi Koleksi)**: `for item in koleksi [do] ... end`.
- **Repeat ... Until Loop (Gaya Lua)**: `repeat ... until <kondisi>`.
- **Counter Loop**: `loop <jumlah> [do] ... end` (mengulang $n$ kali secara terhitung).
- **While Loop**: `while <kondisi> [do] ... end`.
- **Aliran Kontrol**: `break` untuk keluar dari loop seketika, dan `continue` untuk melompati sisa iterasi saat ini.

```fasthon
# 1. Numeric For Loop
for i = 1, 5 do
    print "Nilai i: " + str(i)
end

# Numeric For dengan custom step atau kata kunci 'to'
for j = 1 to 10 step 2 do
    print "Ganjil: ${j}"
end

# 2. For-In Loop (Iterasi List/Array)
buah = ["Apel", "Jeruk", "Mangga",]
for item in buah do
    print "Buah pilihan: ${item}"
end

# 3. Repeat ... Until Loop (Dijalankan minimal 1 kali hingga kondisi terpenuhi)
k = 0
repeat
    k = k + 1
until k >= 3

# 4. Perulangan loop terhitung
loop 3
    print "Pengulangan loop"
end

# 5. Perulangan while dengan break dan continue
i = 0
while true
    i = i + 1
    if i == 3
        continue  # Lewati angka 3
    end
    if i > 5
        break     # Keluar saat i melebihi 5
    end
    print "Angka: ${i}"
end
```

#### Loop Assignment & `silent_print`
Hasil evaluasi `loop` dapat ditugaskan langsung ke variabel untuk mengumpulkan seluruh outputnya. Gunakan `silent_print` agar memformat dan menampung baris teks tanpa membanjiri layar terminal (karena operasi berkas memerlukan `io`, pastikan memuat `use io`):

```fasthon
use io

halo = loop 3000
    silent_print "kamu manusia apa apa"
end

write "hai.txt" halo
```

### Bab 6: Fungsi, Explicit & Implicit Return, serta Kata Kunci Alternatif (`fn`, `def`, `func`)
Fungsi dapat dideklarasikan dengan `fn`, `def`, `func`, maupun `function`, diikuti nama fungsi dan parameter yang dipisahkan spasi atau kurung `(param)`. Setiap deklarasi fungsi wajib ditutup dengan kata kunci `end`.
- **Implicit & Explicit Return**: Baris terakhir otomatis menjadi nilai balik fungsi, ATAU Anda dapat menggunakan kata kunci `return` untuk mengembalikan nilai secara eksplisit / keluar lebih awal (*early exit*).
- **Fleksibilitas Pemanggilan**: Fungsi dapat dipanggil dengan gaya perintah `add 10 20` maupun gaya kurung `add(10, 20)`.

```fasthon
# Menggunakan kata kunci 'fn' dengan implicit return
fn add a b
    a + b
end

print add 10 20
print add(10, 20)

# Menggunakan 'def' / 'func' dengan 'return' eksplisit
def cekStatus umur
    if umur < 0
        return "Tidak valid"
    elseif umur < 18
        return "Minor"
    end
    return "Adult"
end

print cekStatus(20) # Output: Adult
```

#### Memasukkan Fungsi & Ekspresi ke Variabel (First-Class Functions & Expressions)
Fasthon mendukung penugasan fungsi (`fn` / `def`), ekspresi percabangan (`if-else`), dan loop (`loop`) langsung ke dalam variabel:
- **Multi-baris (Multi-line)**: **Wajib** ditutup dengan kata kunci `end`.
- **Satu baris (Single-line)**: Penutup `end` bersifat **opsional** (boleh dipakai atau tidak).

```fasthon
# 1. Fungsi multi-baris di variabel (wajib pakai 'end' dan mendukung parameter)
kali = fn(x, y)
    return x * y
end
print kali(6, 7)  # Output: 42

# 2. Fungsi 1 baris di variabel (end opsional)
tambah = fn(x, y) return x + y       # Tanpa end
kurang = fn(x, y) return x - y end   # Dengan end
print tambah(10, 20)  # Output: 30
print kurang(50, 20)  # Output: 30

# 3. If-Else expression di variabel
# Multi-baris (wajib pakai 'end'):
status = if x > 10
    "tinggi"
else
    "rendah"
end

# Satu baris (end opsional):
s1 = if x > 10 then "tinggi" else "rendah"
s2 = if x > 10 then "tinggi" else "rendah" end

# 4. Loop expression di variabel
# Multi-baris (wajib pakai 'end'):
l1 = loop 3
    "A"
end

# Satu baris (end opsional):
l2 = loop 2 "B"
l3 = loop 2 "C" end
```

### Bab 7: Pustaka I/O & Input Interaktif (`use io` / `import io`)
Untuk menggunakan fitur input-output berkas dan interaksi pengguna, Anda **wajib** memuat pustaka `io` terlebih dahulu menggunakan `use io` atau `import io` (seperti halnya di Python):

```fasthon
use io

nama = io.input("Siapa nama kamu? ")
umur = input "Berapa umur kamu? "
hobi = io.ask("Apa hobi kamu? ")

io.print "Halo ${nama}, umur ${umur}, hobi ${hobi}"

io.write "catatan.txt", "Belajar Fasthon sangat menyenangkan"
isi = io.read "catatan.txt"
print isi
```

### Bab 8: Objek, State Management & Dot Notation (`new`, `set`, `get`)
Buat objek map/state baru dengan `new`, atur properti dengan `set`, dan ambil nilainya dengan `get` atau notasi titik (`.`):

```fasthon
person = new
set person "role" "Engineer"
set person "level" "Senior"

print get person "role"
print person.level

set person "sapa" (fn nama
    "Halo " + nama)
```

### Bab 9: Jaringan & HTTP Client (`use net`, `net.get`, `net.send`)
Permintaan HTTP GET dapat dilakukan langsung dengan memuat library `net`:

```fasthon
use net

response = net.get "httpbin.org"
print response
```

### Bab 10: Aplikasi Desktop Native Win32 (`use gui`, `app`, `window`, `run`)
Membangun antarmuka jendela desktop native secara langsung:

```fasthon
use gui

app "Aplikasi Saya"
window 800 600
print "Desktop window siap"
run 500
```
*Catatan: `run` menerima parameter timeout dalam milidetik atau berjalan terus hingga jendela ditutup pengguna.*

### Bab 11: Modularitas, Pustaka Bawaan & Cara Membuat Library Sendiri (`use` / `import`)

Fasthon mendukung sistem modularitas modern yang fleksibel menggunakan kata kunci `use` atau `import`.

#### A. Menggunakan Library Bawaan (Built-in Standard Libraries)
Fasthon menyertakan pustaka standar bawaan berkecepatan tinggi:
- **`io`**: Input/output berkas (`read`, `write`, `io.read`, `io.write`) dan input pengguna interaktif (`input`, `io.input`, `io.ask`). Wajib dimuat sebelum digunakan!
- **`math`**: Operasi matematika presisi tinggi (`math.sqrt`, `math.abs`, `math.pow`, `math.pi`, dll.).
- **`time`**: Operasi waktu dan jeda eksekusi (`time.now`, `time.sleep`).
- **`net`** / **`http`**: Komunikasi jaringan dan request HTTP (`net.get`, `net.send`).
- **`gui`**: Antarmuka grafis desktop native Win32 (`gui.app`, `gui.window`, `gui.run`).

Contoh pemanggilan pustaka standar:
```fasthon
import io
import math
import time

angka = 16
akar = math.sqrt(angka)
io.print "Akar dari 16 adalah: " + str(akar)

time.sleep 1000
io.print "Selesai jeda 1 detik"
```

---

#### B. Tutorial: Cara Membuat Library / Modul Sendiri
Membuat library di Fasthon sangat sederhana. Anda cukup membuat berkas `.fsn` baru dan mendefinisikan fungsi, variabel, atau objek yang ingin digunakan kembali oleh program lain.

##### Langkah 1: Buat Berkas Library (Contoh: `kalkulator.fsn`)
Simpan file ini dengan nama `kalkulator.fsn`:
```fasthon
phi = 3.14159

fn tambah a b
    a + b

fn kurang a b
    a - b

fn kali a b
    a * b

fn bagi a b
    a / b

fn luas_lingkaran r
    phi * r * r
```

> 💡 **Tips Pengorganisasian dengan Objek (Namespacing):**
> Anda juga dapat membungkus fungsi-fungsi library ke dalam suatu objek:
> ```fasthon
> sapaFn = fn nama
>     "Halo " + nama
> 
> helper = new
> set helper "sapa" sapaFn
> ```

---

#### C. Tutorial: Cara Mengimpor & Menggunakan Library
Gunakan perintah `import` atau `use` pada skrip utama Anda.

##### Langkah 2: Buat Skrip Utama (Contoh: `main.fsn`)
Simpan di direktori yang sama dengan `kalkulator.fsn`:
```fasthon
use io
import "kalkulator"

hasilTambah = tambah 15 25
io.print "15 + 25 = ${hasilTambah}"

hasilKali = kali(6, 7)
io.print "6 * 7 = ${hasilKali}"

r = 10
luas = luas_lingkaran r
io.print "Luas lingkaran (r=10): ${luas}"
```

##### Berbagai Format Penulisan Impor yang Didukung:
Fasthon memberikan fleksibilitas tinggi dalam cara pemanggilan modul:
1. **Dengan Tanda Kutip**:
   - `import "kalkulator"` atau `use "kalkulator"`
   - `import "kalkulator.fsn"` atau `use "kalkulator.fsn"`
2. **Tanpa Tanda Kutip (Gaya Python / Ruby)**:
   - `import kalkulator` atau `use kalkulator`
3. **Fleksibilitas Pemanggilan Fungsi**:
   - Fungsi dari library yang diimpor dapat dipanggil dengan spasi tanpa tanda kurung (`tambah 10 20`) ataupun dengan tanda kurung (`tambah(10, 20)`).

---

### Bab 12: Fungsi Bawaan Global & Konversi Tipe Data

Fasthon menyediakan fungsi bawaan global tingkat sistem yang dapat digunakan langsung tanpa perlu memuat pustaka tambahan:

#### 1. Memahami Tipe Data & Fungsi Konversi `int` (Integer 64-bit)

##### A. Apa itu `int` di Fasthon?
`int` (kependekan dari *integer*) adalah **tipe data bilangan bulat 64-bit bertanda (*signed 64-bit integer*)** di Fasthon. Karakteristik utamanya:
- **Bilangan Bulat Murni**: Tidak memiliki titik/koma desimal, baik bernilai positif, nol, maupun negatif (contoh: `-100`, `0`, `42`, `1000000`).
- **Kapasitas Ekstra Besar (64-bit)**: Memiliki rentang nilai dari **$-9.223.372.036.854.775.808$ hingga $+9.223.372.036.854.775.807$** (setara dengan `int64_t` di C/C++), sehingga sangat aman dari masalah *integer overflow* pada perhitungan komputasi umum maupun finansial.
- **Kinerja Ultra-Tinggi (Zero Overhead)**: Di dalam Virtual Machine dan kompilasi biner native AOT Fasthon, tipe `int` diproses secara langsung (*unboxed*) pada register CPU 64-bit tanpa alokasi memori tambahan di heap.

##### B. Fungsi Bawaan `int(val)` & Kapan Harus Digunakan
Secara bawaan, masukan dari pengguna (`input()`) dan isi berkas teks (`read()`) dibaca sebagai teks bertipe `string`. Di sinilah fungsi `int(val)` sangat penting:

1. **Mencegah Kesalahan Penggabungan Teks (*String Concatenation Bug*)**:
   Jika dua variabel bertipe teks dijumlahkan dengan operator `+`, Fasthon akan menyambungkan teksnya:
   ```fasthon
   a = "10"
   b = "20"
   print a + b          # Output: "1020" (penggabungan teks, BUKAN penjumlahan matematika!)
   print int(a) + int(b) # Output: 30 (penjumlahan matematika sejati setelah dikonversi ke int)
   ```
2. **Memperbaiki Perbandingan Logika Numerik**:
   Perbandingan string dilakukan secara alfabetis (leksikografis), di mana karakter `"1"` dianggap lebih kecil daripada `"2"`, sehingga `"100" < "20"` menghasilkan `true`. Dengan mengonversinya ke `int`:
   ```fasthon
   print "100" < "20"         # Output: true (salah secara matematika karena perbandingan alfabetis)
   print int("100") < int("20") # Output: false (benar, 100 lebih besar dari 20)
   ```
3. **Memotong Angka Pecahan (*Truncation* dari `float`)**:
   Jika mengonversi angka desimal berpecahan (`float`), fungsi `int()` akan membuang bagian pecahan di belakang koma:
   ```fasthon
   print int(9.85)   # Output: 9
   print int(-4.7)   # Output: -4
   ```
4. **Konversi dari Boolean**:
   Mengonversi status kebenaran menjadi representasi numerik biner:
   ```fasthon
   print int(true)   # Output: 1
   print int(false)  # Output: 0
   ```

##### C. Fungsi Konversi Lainnya: `str(val)` & `float(val)`
- **`str(val)`**: Mengonversi nilai apa pun (angka, boolean, list, objek) menjadi bentuk teks string. Berguna untuk digabungkan dengan pesan atau disimpan ke berkas.
- **`float(val)`**: Mengonversi teks string atau integer menjadi angka desimal presisi ganda (*64-bit double precision float*).

```fasthon
use io

# Contoh Praktis Aplikasi Kasir:
hargaTeks = "25000"
jumlahTeks = "3"

total = int(hargaTeks) * int(jumlahTeks)
print "Total Belanja: Rp " + str(total)  # Output: Total Belanja: Rp 75000
```

#### 2. Inspeksi Ukuran & Koleksi Data
- **`len(target)`**: Menghitung panjang karakter string, jumlah elemen list, atau jumlah kunci pada objek map.
- **`push(list, val)`**: Menambahkan elemen ke urutan terakhir list.
- **`pop(list)`**: Mengambil dan menghapus elemen terakhir list.

```fasthon
# Mengetahui panjang string & list
print len("Fasthon")          # Output: 7

daftar = [10, 20]
push(daftar, 30)
print len(daftar)             # Output: 3
print pop(daftar)             # Output: 30
```

#### 3. Manipulasi & Pencocokan String
- **`lower(str)`**: Mengubah teks menjadi huruf kecil (*lowercase*).
- **`upper(str)`**: Mengubah teks menjadi huruf kapital (*uppercase*).
- **`case_sensitive(a, b)`**: Memeriksa apakah dua teks sama persis dengan membedakan huruf kapital.
- **`incase_sensitive(a, b)`**: Memeriksa apakah dua teks sama dengan menyamakan huruf kapital/non-kapital (*case-insensitive*).

```fasthon
kalimat = "Belajar Fasthon"
print lower(kalimat)          # Output: belajar fasthon
print upper(kalimat)          # Output: BELAJAR FASTHON

user1 = "admin"
user2 = "ADMIN"
print incase_sensitive(user1, user2)  # Output: true
print case_sensitive(user1, user2)    # Output: false
```

---

### Bab 13: Tutorial Kilat & Panduan Lengkap Seluruh Modul Bawaan (Standard Library Deep Dive)

Fasthon menganut filosofi *batteries-included*, di mana pustaka-pustaka esensial untuk kebutuhan komputasi matematika, I/O berkas & input keyboard, pengukuran waktu, jaringan HTTP, dan antarmuka jendela desktop native telah terintegrasi langsung di dalam engine tanpa perlu mengunduh package manager eksternal. Anda cukup mengaktifkannya menggunakan keyword `use <nama_modul>` atau `import <nama_modul>`.

---

#### 1. Modul `math` (Matematika, Trigonometri & Random Number Generator)
Modul `math` menyediakan fungsi-fungsi perhitungan matematika berkecepatan tinggi, operasi trigonometri, konstanta fundamental, serta generator angka acak presisi tinggi (PRNG berbasis Mersenne Twister 64-bit `std::mt19937_64`).

##### A. Kamus Fungsi Modul `math`
| Fungsi / Properti | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `math.random(min, max)` | Menghasilkan angka integer acak antara `min` dan `max` (inklusif). Jika tanpa argumen, menghasilkan float acak $0.0 \le x < 1.0$. | `dadu = math.random(1, 6)` |
| `math.random_seed(seed)` *(atau `math.seed`)* | Mengatur *seed* generator acak dengan nilai integer tertentu untuk menghasilkan urutan acak yang **deterministik / dapat diulang**. Sangat berguna untuk pengujian unit, savegame, atau procedural generation. | `math.random_seed(42)` |
| `math.random_seed()` | Jika dipanggil tanpa argumen, fungsi ini akan me-reset seed menggunakan **entropi hardware murni** (`std::random_device`) yang digabung dengan clock prosesor beresolusi tinggi, sehingga angka acak berikutnya dijamin tak terduga (*unpredictable*). | `math.random_seed()` |
| `math.sqrt(x)` | Menghitung akar kuadrat dari $x$. | `math.sqrt(81)` $\rightarrow$ `9.0` |
| `math.pow(base, exp)` | Menghitung perpangkatan $base^{exp}$. | `math.pow(2, 8)` $\rightarrow$ `256.0` |
| `math.abs(x)` | Mengembalikan nilai absolut positif dari $x$. | `math.abs(-25)` $\rightarrow$ `25` |
| `math.floor(x)` | Membulatkan angka pecahan ke bawah ke integer terdekat. | `math.floor(4.9)` $\rightarrow$ `4` |
| `math.ceil(x)` | Membulatkan angka pecahan ke atas ke integer terdekat. | `math.ceil(4.1)` $\rightarrow$ `5` |
| `math.round(x)` | Membulatkan angka desimal ke integer terdekat (standar matematika). | `math.round(4.6)` $\rightarrow$ `5` |
| `math.min(a, b)` | Mengembalikan nilai terkecil di antara dua angka. | `math.min(10, 5)` $\rightarrow$ `5` |
| `math.max(a, b)` | Mengembalikan nilai terbesar di antara dua angka. | `math.max(10, 5)` $\rightarrow$ `10` |
| `math.sin(rad)` | Menghitung nilai sinus (sudut dalam radian). | `math.sin(0)` $\rightarrow$ `0.0` |
| `math.cos(rad)` | Menghitung nilai kosinus (sudut dalam radian). | `math.cos(0)` $\rightarrow$ `1.0` |
| `math.tan(rad)` | Menghitung nilai tangen (sudut dalam radian). | `math.tan(0)` $\rightarrow$ `0.0` |
| `math.asin(x)` | Menghitung arc sinus dalam radian. | `math.asin(1.0)` |
| `math.acos(x)` | Menghitung arc kosinus dalam radian. | `math.acos(1.0)` |
| `math.atan(x)` | Menghitung arc tangen dalam radian. | `math.atan(1.0)` |
| `math.atan2(y, x)` | Menghitung arc tangen dari kuadran $y / x$. | `math.atan2(1.0, 1.0)` |
| `math.sinh(x)` / `cosh` / `tanh` | Menghitung fungsi hiperbolik sinus, kosinus, tangen. | `math.sinh(1.0)` |
| `math.exp(x)` | Menghitung nilai eksponensial $e^x$. | `math.exp(1)` |
| `math.log(x)` | Menghitung logaritma natural basis $e$. | `math.log(math.e)` $\rightarrow$ `1.0` |
| `math.log10(x)` | Menghitung logaritma basis 10. | `math.log10(100)` $\rightarrow$ `2.0` |
| `math.log2(x)` | Menghitung logaritma basis 2. | `math.log2(8)` $\rightarrow$ `3.0` |
| `math.degrees(rad)` | Mengonversi radian ke derajat sudut. | `math.degrees(math.pi)` $\rightarrow$ `180.0` |
| `math.radians(deg)` | Mengonversi derajat sudut ke radian. | `math.radians(180)` $\rightarrow$ `3.14159...` |
| `math.hypot(x, y)` | Menghitung panjang hipotenusa $\sqrt{x^2 + y^2}$. | `math.hypot(3, 4)` $\rightarrow$ `5.0` |
| `math.trunc(x)` | Memotong bagian pecahan dari angka desimal. | `math.trunc(3.7)` $\rightarrow$ `3` |
| `math.clamp(x, min, max)` | Membatasi nilai $x$ dalam rentang $[min, max]$. | `math.clamp(15, 0, 10)` $\rightarrow$ `10` |
| `math.pi` | Konstanta $\pi$ bernilai $3.141592653589793...$ | `keliling = 2 * math.pi * r` |
| `math.tau` | Konstanta $\tau = 2\pi$ bernilai $6.283185307179...$ | `math.tau` |
| `math.e` | Konstanta bilangan Euler $e$ bernilai $2.718281828459...$ | `math.e` |
| `math.inf` | Nilai floating-point tak hingga (*infinity*). | `math.inf` |

##### B. Contoh Praktis: Mengontrol Random Seed & Game Tebak Angka
Secara default, saat Anda menjalankan Fasthon, generator angka acak **otomatis menggunakan entropi perangkat keras** sehingga setiap kali program dijalankan, angka tebakan akan selalu berbeda dan tidak bisa diprediksi. Namun jika Anda membutuhkan hasil yang konsisten (misalnya saat testing), Anda dapat menetapkan seed secara manual:

```fasthon
use io
use math

# --- 1. Contoh Seed Deterministik (Hasil selalu sama setiap kali dijalankan) ---
math.random_seed(12345)
print "Acak 1: ${math.random(1, 100)}"
print "Acak 2: ${math.random(1, 100)}"

# --- 2. Reset ke Entropi Hardware Murni (Tidak bisa ditebak) ---
math.random_seed()

# --- 3. Aplikasi: Game Tebak Angka Interaktif ---
angkaRahasia = math.random(1, 100)
selesai = false
percobaan = 0

print "🎯 GAME TEBAK ANGKA RESMI FASTHON"
print "Komputer telah memilih angka acak antara 1 s.d. 100!"

while selesai == false
    percobaan = percobaan + 1
    tebakan = int(input("Masukkan tebakan ke-${percobaan}: "))

    if tebakan < angkaRahasia
        print "📉 Terlalu KECIL! Coba angka yang lebih besar."
    elseif tebakan > angkaRahasia
        print "📈 Terlalu BESAR! Coba angka yang lebih kecil."
    else
        print "🎉 SELAMAT! Tebakanmu BENAR!"
        print "Angkanya adalah: ${angkaRahasia}"
        print "Kamu berhasil menebak dalam ${percobaan} kali percobaan."
        selesai = true
```

---

#### 2. Modul `io` (Input Keyboard Interaktif & File I/O Native)
Modul `io` mengelola interaksi konsol serta pembacaan dan penulisan berkas pada filesystem secara cepat dan aman.

##### A. Kamus Fungsi Modul `io`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `input(prompt)` *(atau `io.input`)* | Menampilkan teks pertanyaan ke layar konsol dan membaca satu baris input dari pengguna | `nama = input("Nama Anda: ")` |
| `ask(prompt)` *(atau `io.ask`)* | Alternatif penulisan semantik dari fungsi `input` | `umur = ask("Umur: ")` |
| `write(filepath, teks)` *(atau `io.write`)* | Menulis seluruh teks string ke berkas secara native dengan internal buffer 256 KB (otomatis menimpa/membuat baru) | `write "pesan.txt" "Halo Dunia"` |
| `append(filepath, teks)` *(atau `io.append`)* | Menambahkan teks ke akhir berkas secara efisien tanpa menimpa (*append mode*) | `append "log.txt" "Baris baru\n"` |
| `read(filepath)` *(atau `io.read`)* | Membaca seluruh isi berkas teks dari disk secara instan ke dalam memori | `konten = read "pesan.txt"` |
| `io.open(filepath, mode)` *(atau `open`)* | Membuka stream berkas dengan internal buffer 256 KB. Mengembalikan objek file dengan method: `f.writeline(teks)`, `f.write(teks)`, `f.flush()`, `f.close()` untuk menulis ratusan ribu baris secepat kilat | `f = io.open("data.txt", "w")` |
| `io.write_lines(filepath, list)` | Menulis seluruh elemen list string ke berkas sekaligus dalam satu operasi native tercepat | `io.write_lines("data.txt", barisList)` |
| `io.print(...)` | Menampilkan teks output konsol yang terisolasi di dalam namespace `io` | `io.print "Data berhasil disimpan"` |

##### B. Contoh Praktis: Sistem Catatan & Penyimpanan Skor (High-Score Logger)
```fsn
use io

print "=== SISTEM PENCATAT SKOR GAME ==="
pemain = input("Masukkan Nama Pemain: ")
skor = input("Masukkan Skor Tertinggi: ")

# Menulis data ke berkas teks di disk
catatan = "Pemain: ${pemain} | Skor: ${skor}\n"
write "leaderboard.txt" catatan
print "✅ Data pemain berhasil disimpan ke leaderboard.txt"

# Membaca kembali berkas dari disk
print "\n📄 Isi Berkas leaderboard.txt Saat Ini:"
isi = read "leaderboard.txt"
print isi

# --- Menulis 100.000 Baris Super Cepat dengan io.open() Stream Buffer ---
f = io.open("transaksi.log", "w")
i = 0
while i < 100000
    f.writeline("Log transaksi #${i} status=SUCCESS")
    i = i + 1
f.close()
print "✅ 100.000 baris log transaksi berhasil ditulis dalam hitungan milidetik!"
```

---

#### 3. Modul `time` (Waktu Presisi Tinggi & Delay Eksekusi)
Modul `time` berguna untuk mengukur performa baris kode (profiling / stopwatch), jeda proses (*sleeping/delay*), atau penanda waktu UNIX.

##### A. Kamus Fungsi Modul `time`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `time.now()` | Mengembalikan waktu epoch saat ini dalam satuan **milidetik** (integer 64-bit) | `mulai = time.now()` |
| `time.time()` | Mengembalikan epoch saat ini dalam satuan **detik** desimal (floating-point) | `t = time.time()` |
| `time.time_ns()` | Mengembalikan epoch saat ini dalam satuan **nanodetik** (integer 64-bit) | `ns = time.time_ns()` |
| `time.sleep(ms)` | Menghentikan sementara (*pause*) eksekusi program selama $ms$ milidetik | `time.sleep 1000` *(jeda 1 detik)* |
| `time.sleep_s(sec)` | Menghentikan sementara program selama $sec$ detik | `time.sleep_s(1.5)` |
| `time.format(fmt)` | Memformat waktu sistem lokal dengan format `strftime` (default: `"%Y-%m-%d %H:%M:%S"`) | `waktu = time.format()` |

##### B. Contoh Praktis: Pengukur Durasi Eksekusi & Hitung Mundur
```fasthon
use io
use time

print "⏳ Memulai Hitung Mundur..."
loop 3
    print "Detik..."
    time.sleep 1000  # Jeda 1.000 milidetik (1 detik)

# Mengukur kecepatan komputasi algoritma (Benchmark stopwatch)
waktuAwal = time.now()

total = 0
i = 0
while i < 1000000
    i = i + 1
    total = total + i

waktuAkhir = time.now()
durasi = waktuAkhir - waktuAwal

print "Hasil penjumlahan 1 s.d 1.000.000: ${total}"
print "⚡ Waktu komputasi yang dibutuhkan: ${durasi} ms"
```

---

#### 4. Modul `net` / `http` (HTTP Client & Komunikasi Jaringan)
Modul `net` (bisa juga diakses dengan alias `http`) memungkinkan program Fasthon melakukan permintaan jaringan berbasis protokol HTTP GET dan POST secara langsung.

##### A. Kamus Fungsi Modul `net` / `http`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `net.get(url)` *(atau `http.get`)* | Mengirimkan permintaan HTTP GET ke alamat URL yang ditentukan dan mengembalikan respons teks dari server | `resp = net.get "httpbin.org/get"` |
| `net.send(url, payload)` *(atau `http.send`)* | Mengirimkan data teks/payload via HTTP POST ke endpoint tujuan | `res = net.send "api.site/data" "key=val"` |

##### B. Contoh Praktis: Mengambil Data dari REST API
```fasthon
use io
use net

print "🌐 Mengambil data dari server API..."
data = net.get "httpbin.org/uuid"
print "Respons dari Server:"
print data
```

---

#### 5. Modul `gui` (Aplikasi Antarmuka Jendela Desktop Native Win32)
Modul `gui` menyediakan API minimalis berkinerja tinggi untuk memunculkan antarmuka jendela desktop native pada sistem operasi Windows tanpa memerlukan instalasi library UI pihak ketiga yang berat.

##### A. Kamus Fungsi Modul `gui`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `gui.app(judul)` *(atau `app judul`)* | Menginisialisasi aplikasi GUI dan menentukan judul bilah jendela (*window title*) | `gui.app "Kalkulator Fasthon"` |
| `gui.window(lebar, tinggi)` *(atau `window w h`)* | Mengatur dimensi ukuran jendela dalam piksel | `gui.window 800 600` |
| `gui.run(timeout_ms)` *(atau `run`)* | Menjalankan message loop native. Jika diberi parameter milidetik, jendela akan otomatis tertutup setelah waktu habis | `gui.run 3000` *(tampil selama 3 detik)* |

##### B. Contoh Praktis: Memunculkan Jendela GUI Desktop
```fasthon
use io
use gui

print "Mempersiapkan jendela native..."
gui.app "Dashboard Monitoring Fasthon"
gui.window 1024 768
print "Jendela berhasil dibuat di layar pengguna!"

# Menjalankan lifecycle window selama 5 detik (atau hingga ditutup pengguna)
gui.run 5000
```

---

#### 6. Modul `str` (Manipulasi Teks Tingkat Lanjut)
Meskipun fungsi manipulasi teks umum tersedia secara global, modul `str` menyediakan namespace terstruktur untuk pengelolaan dan standardisasi string dalam proyek besar.

##### A. Kamus Fungsi Modul `str`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `str.lower(teks)` | Mengubah seluruh abjad dalam teks menjadi huruf kecil | `str.lower("Fasthon")` $\rightarrow$ `"fasthon"` |
| `str.upper(teks)` | Mengubah seluruh abjad dalam teks menjadi huruf kapital | `str.upper("Fasthon")` $\rightarrow$ `"FASTHON"` |
| `str.split(teks, pemisah)` | Memotong teks menjadi list string berdasarkan delimiter | `str.split("a,b,c", ",")` $\rightarrow$ `["a", "b", "c"]` |
| `str.join(pemisah, list)` | Menggabungkan list string menjadi satu string dengan pemisah | `str.join("-", ["x", "y"])` $\rightarrow$ `"x-y"` |
| `str.replace(teks, lama, baru)` | Mengganti kemunculan substring lama dengan substring baru | `str.replace("halo dunia", "dunia", "fasthon")` |
| `str.trim(teks)` | Menghapus whitespace (spasi, newline, tab) di awal dan akhir teks | `str.trim("  halo  ")` $\rightarrow$ `"halo"` |
| `str.startswith(teks, awalan)` | Memeriksa apakah teks diawali dengan prefix tertentu | `str.startswith("fasthon.fsn", "fasthon")` |
| `str.endswith(teks, akhiran)` | Memeriksa apakah teks diakhiri dengan suffix tertentu | `str.endswith("skrip.fsn", ".fsn")` |
| `str.contains(teks, sub)` | Memeriksa apakah teks mengandung substring | `str.contains("abcde", "bcd")` |
| `str.find(teks, sub)` | Mencari indeks pertama kemunculan substring (-1 jika tidak ada) | `str.find("halo", "lo")` $\rightarrow$ `2` |
| `str.case_sensitive(a, b)` | Membandingkan kecocokan dua teks secara ketat dengan membedakan huruf besar/kecil | `str.case_sensitive("A", "a")` $\rightarrow$ `false` |
| `str.incase_sensitive(a, b)` | Membandingkan kecocokan dua teks tanpa mempedulikan huruf besar/kecil (*case-insensitive*) | `str.incase_sensitive("Admin", "admin")` $\rightarrow$ `true` |

##### B. Contoh Praktis: Validasi Autentikasi Form
```fasthon
use io
use str

usernameDB = "admin_super"
passwordDB = "Rahasia123!"

inputUser = input("Username: ")
inputPass = input("Password: ")

# Username bersifat case-insensitive (huruf besar/kecil sama saja)
userCocok = str.incase_sensitive(inputUser, usernameDB)

# Password bersifat case-sensitive (huruf besar/kecil wajib persis sama)
passCocok = str.case_sensitive(inputPass, passwordDB)

if userCocok and passCocok
    print "🔓 Akses DITERIMA! Selamat datang, Administrator."
elseif userCocok and not passCocok
    print "❌ Akses DITOLAK: Password salah (perhatikan huruf besar/kecil)!"
else
    print "❌ Akses DITOLAK: Akun pengguna tidak ditemukan!"
```

---

#### 7. Modul `os` & `path` (Sistem Operasi, Filesystem, Environment & Subproses)
Modul `os` dan submodul `path` (atau `os.path`) menyediakan antarmuka lengkap ke sistem operasi host: manajemen direktori dan file, manipulasi path lintas-platform, pembacaan environment variable, serta eksekusi proses shell/terminal.

##### A. Kamus Fungsi Modul `os` & `path`
| Fungsi / Properti | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `os.name` | Nama sistem operasi saat ini (`"posix"` atau `"nt"`) | `if os.name == "posix"` |
| `os.sep` | Karakter pemisah path (`"/"` di Linux/Unix, `"\\"` di Windows) | `path = dir + os.sep + file` |
| `os.linesep` | Karakter pemisah baris baru (`"\n"` atau `"\r\n"`) | `os.linesep` |
| `os.devnull` | Path perangkat null (`"/dev/null"` atau `"NUL"`) | `os.devnull` |
| `os.getcwd()` | Mengembalikan direktori kerja saat ini (*current working directory*) | `cwd = os.getcwd()` |
| `os.chdir(path)` | Berpindah direktori kerja aktif | `os.chdir("../projek")` |
| `os.listdir(path)` | Mengembalikan list nama berkas/folder di direktori target | `berkas = os.listdir(".")` |
| `os.mkdir(path)` | Membuat direktori baru | `os.mkdir("folder_baru")` |
| `os.makedirs(path)` | Membuat direktori beserta seluruh sub-direktori induknya secara rekursif | `os.makedirs("a/b/c")` |
| `os.remove(path)` / `unlink` | Menghapus sebuah berkas | `os.remove("temp.txt")` |
| `os.rmdir(path)` | Menghapus direktori kosong | `os.rmdir("folder_lama")` |
| `os.rmtree(path)` | Menghapus direktori beserta seluruh isinya secara rekursif | `os.rmtree("cache_folder")` |
| `os.rename(src, dst)` | Mengubah nama atau memindahkan berkas / direktori | `os.rename("old.txt", "new.txt")` |
| `os.system(cmd)` | Menjalankan perintah shell dan mengembalikan exit code status integer | `code = os.system("ls -la")` |
| `os.popen(cmd)` | Menjalankan perintah shell dan menangkap seluruh teks output stdout | `output = os.popen("uname -a")` |
| `os.getenv(key, default)` | Membaca nilai *environment variable* host | `port = os.getenv("PORT", "8080")` |
| `os.setenv(key, val)` | Menetapkan nilai *environment variable* untuk sesi saat ini | `os.setenv("ENV", "production")` |
| `os.unsetenv(key)` | Menghapus variabel environment | `os.unsetenv("TOKEN")` |
| `os.getpid()` | Mendapatkan Process ID (PID) program Fasthon saat ini | `pid = os.getpid()` |
| `os.cpu_count()` | Mendapatkan jumlah core prosesor logis yang tersedia | `cores = os.cpu_count()` |
| `os.exit(code)` | Keluar dari program seketika dengan status kode tertentu | `os.exit(0)` |
| `path.exists(path)` *(atau `os.path.exists`)* | Memeriksa apakah berkas atau folder target ada | `if path.exists("config.json")` |
| `path.isfile(path)` | Memeriksa apakah path merupakan berkas reguler | `path.isfile("data.txt")` |
| `path.isdir(path)` | Memeriksa apakah path merupakan sebuah direktori | `path.isdir("src")` |
| `path.getsize(path)` | Mengembalikan ukuran berkas dalam satuan byte | `size = path.getsize("foto.jpg")` |
| `path.join(...)` | Menggabungkan fragmen path dengan separator platform otomatis | `p = path.join("dir", "sub", "file.txt")` |
| `path.basename(path)` | Mengambil nama file terakhir dari path | `path.basename("/home/user/a.txt")` $\rightarrow$ `"a.txt"` |
| `path.dirname(path)` | Mengambil direktori induk dari path | `path.dirname("/home/user/a.txt")` $\rightarrow$ `"/home/user"` |
| `path.abspath(path)` | Mengonversi path relatif menjadi path absolut sistem | `abs = path.abspath(".")` |
| `path.isabs(path)` | Memeriksa apakah path berupa path absolut | `path.isabs("/etc")` $\rightarrow$ `true` |

##### B. Contoh Praktis: Otomasi File & Direktori
```fasthon
use os

print "Direktori saat ini: ${os.getcwd()}"
print "Jumlah Core CPU: ${os.cpu_count()}"

if not os.path.exists("backup")
    os.makedirs("backup/data")
    print "Direktori backup berhasil dibuat!"

# Menjalankan perintah terminal dan menangkap output
output = os.popen("echo Fasthon Berjalan!")
print "Output shell: ${output}"
```

---

#### 8. Modul `sys` (Informasi Sistem, CLI Arguments, & Stream I/O)
Modul `sys` menyediakan akses ke argumen baris perintah (`sys.argv`), platform runtime, identifikasi PID, dan stream output standar berkecepatan tinggi.

##### A. Kamus Fungsi Modul `sys`
| Fungsi / Properti | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `sys.argv` | List berisi seluruh argumen command line yang diteruskan ke program | `args = sys.argv` |
| `sys.platform` | Nama platform (`"linux"`, `"win32"`, `"darwin"`, dll.) | `print sys.platform` |
| `sys.version` | Versi FasthonJIT saat ini | `print sys.version` |
| `sys.stdout_write(teks)` | Menulis teks ke stdout murni tanpa newline otomatis | `sys.stdout_write("Loading...")` |
| `sys.stderr_write(teks)` | Menulis teks pesan kesalahan ke stderr | `sys.stderr_write("Error!\n")` |
| `sys.getpid()` | Mendapatkan Process ID program | `pid = sys.getpid()` |
| `sys.exit(code)` | Menghentikan program dengan status code | `sys.exit(1)` |

##### B. Contoh Praktis: Mengolah Argumen Command Line
```fasthon
use sys

if len(sys.argv) < 2
    sys.stderr_write("Penggunaan: fasthon program.fsn <nama>\n")
    sys.exit(1)

nama = sys.argv[1]
print "Halo, ${nama}! Anda berjalan di ${sys.platform}."
```

---

#### 9. Modul `json` (Serialisasi & Deserialisasi Objek/List)
Modul `json` mempermudah pertukaran data modern (REST API, konfigurasi berkas, serialisasi struktur data) dengan konversi dua arah antara teks JSON dan tipe data bawaan Fasthon (list, object, string, number, boolean, null).

##### A. Kamus Fungsi Modul `json`
| Fungsi | Penjelasan | Contoh Penggunaan |
| :--- | :--- | :--- |
| `json.dumps(val)` *(atau `json.stringify`)* | Mengonversi nilai Fasthon (string, number, boolean, list, object) menjadi string berformat JSON | `jsonStr = json.dumps(data)` |
| `json.loads(str)` *(atau `json.parse`)* | Mem-parsing string JSON menjadi tipe data asli Fasthon (list / object / number / string) | `data = json.loads(jsonStr)` |

##### B. Contoh Praktis: Serialisasi & Parsing Konfigurasi JSON
```fasthon
use json

# Membuat konfigurasi menggunakan objek Fasthon
config = new
config["server"] = "localhost"
config["port"] = 8080
config["debug"] = true
config["endpoints"] = ["/api/v1", "/api/v2"]

# Serialisasi ke format JSON
teksJson = json.dumps(config)
print "Hasil JSON:"
print teksJson

# Membaca kembali (parsing) dari teks JSON
parsed = json.loads(teksJson)
print "Server: ${parsed[\"server\"]}"
print "Port: ${parsed[\"port\"]}"
print "Endpoint pertama: ${parsed[\"endpoints\"][0]}"
```

---

## 4. Kamus Keyword, Fungsi Bawaan & Operator

### 4.1 Kamus Keyword Utama

| Keyword | Kategori | Fungsi & Keterangan | Contoh Kode |
| :--- | :--- | :--- | :--- |
| `print` | Output | Menampilkan satu atau beberapa nilai ke konsol standar dengan spasi pemisah dan diakhiri baris baru | `print "Halo" 123` |
| `silent_print` | Output | Memformat teks seperti `print` tanpa mencetak ke konsol terminal (menampung output untuk loop assignment) | `silent_print "Data"` |
| `if` | Logika | Memulai blok percabangan kondisional berbasis indentasi bersih | `if score > 75` |
| `then` | Logika | Penanda opsional setelah kondisi percabangan `if` (gaya Lua/Pascal) | `if score > 75 then` |
| `elseif` / `elif` | Logika | Percabangan alternatif multikondisi jika kondisi sebelumnya tidak terpenuhi | `elseif score >= 60` |
| `else` | Logika | Blok alternatif jika seluruh kondisi `if` / `elseif` sebelumnya salah | `else` |
| `for` | Iterasi | Perulangan numerik dengan batas rentang (`for i = 1, 10`) atau perulangan iterasi koleksi list (`for item in list`) | `for i = 1, 5 do` |
| `in` | Iterasi | Menentukan koleksi list yang akan diiterasi pada for-in loop | `for item in list do` |
| `to` / `step` | Iterasi | Kata kunci kontekstual penentu batas akhir dan langkah lompatan pada numeric for loop | `for i = 1 to 10 step 2 do` |
| `repeat` | Iterasi | Perulangan gaya Lua yang berjalan minimal 1 kali hingga kondisi `until` terpenuhi | `repeat x = x + 1 until x >= 5` |
| `until` | Iterasi | Menutup blok `repeat` dan mengevaluasi kondisi terminasi perulangan | `until count == 10` |
| `loop` | Iterasi | Mengulang eksekusi blok sebanyak $n$ kali secara terhitung | `loop 10` |
| `while` | Iterasi | Mengulang eksekusi blok selama ekspresi kondisional bernilai benar | `while x > 0` |
| `do` | Struktur | Penanda opsional pembuka blok loop (`for`, `while`, `loop`) atau fungsi | `while x > 0 do` |
| `break` | Kontrol Loop | Menghentikan eksekusi perulangan (`for`, `while`, `loop`, `repeat`) dan keluar seketika | `break` |
| `continue` | Kontrol Loop | Melompati sisa baris iterasi saat ini dan langsung ke iterasi loop berikutnya | `continue` |
| `var` / `let` | Variabel | Kata kunci deklarasi variabel eksplisit opsional (gaya JS/Go/Swift/Rust) | `var a = 10` atau `let b = 20` |
| `return` | Fungsi | Mengembalikan nilai dari fungsi secara eksplisit / keluar lebih awal | `return hasil` |
| `fn` / `def` / `func` / `function` | Fungsi | Mendeklarasikan fungsi baru (mendukung implicit dan explicit return) | `def calc a b` atau `fn x` |
| `use` / `import` / `include` / `require` | Modul | Mengimpor berkas modul `.fsn` eksternal atau pustaka bawaan sistem | `use io` atau `import math` |
| `new` | Objek | Menginstansiasi objek map/state baru di memori | `user = new` |
| `get` | Objek / HTTP | Mengambil properti objek (`get obj "key"`) atau melakukan HTTP GET (`get "url"`) | `get user "name"` |
| `set` | Objek | Menetapkan nilai properti pada objek/map (`set obj "key" val`) | `set user "age" 25` |
| `read` | File I/O | Membaca seluruh isi berkas teks (membutuhkan `use io` / `import io`) | `text = read "data.txt"` |
| `write` | File I/O | Menulis teks ke berkas target secara native (membutuhkan `use io` / `import io`) | `write "data.txt" "Konten"` |
| `send` | Jaringan | Mengirimkan data teks/payload ke URL tujuan (HTTP POST / Socket) | `send "url" payload` |
| `app` | Desktop GUI | Menetapkan judul untuk aplikasi jendela desktop (`use gui`) | `app "Title Window"` |
| `window` | Desktop GUI | Mengatur lebar dan tinggi jendela GUI desktop native (`use gui`) | `window 1024 768` |
| `run` | Desktop GUI | Memulai message pump dan lifecycle aplikasi desktop native (`use gui`) | `run` atau `run 1000` |
| `end` | Struktur | Kata kunci penutup blok kontrol (`if`, `for`, `while`, `loop`, `def`/`fn`) bergaya Lua | `end` |
| `true` / `True` / `TRUE` | Nilai | Literal boolean benar | `isAktif = true` |
| `false` / `False` / `FALSE` | Nilai | Literal boolean salah | `isAktif = false` |
| `nil` / `null` / `None` | Nilai | Literal nilai kosong / ketiadaan nilai | `data = nil` |

### 4.2 Kamus Fungsi Bawaan Global (Built-in Functions)

Fungsi-fungsi ini dapat dipanggil langsung dari mana saja tanpa perlu import/use:

| Fungsi | Parameter | Nilai Balik | Keterangan & Contoh |
| :--- | :--- | :--- | :--- |
| `int(x)` | Angka, String, Boolean | `int` | Mengonversi nilai menjadi integer 64-bit bertanda murni (memotong desimal float, mengubah teks string angka ke nilai komputasi; panduan lengkap ada di Bab 12) |
| `float(x)` | Angka, String | `float` | Mengonversi nilai menjadi angka pecahan floating-point 64-bit presisi ganda (`float("3.14")` $\rightarrow$ `3.14`) |
| `str(x)` | Nilai apa saja | `string` | Mengonversi nilai apa pun menjadi string teks (`str(123)` $\rightarrow$ `"123"`) |
| `len(x)` | String, List, Objek | `int` | Menghitung panjang teks, jumlah item list, atau jumlah properti objek (`len("Halo")` $\rightarrow$ `4`) |
| `push(list, val)` | List, Nilai baru | `val` | Menambahkan elemen baru ke akhir list (`push(buah, "Apel")`) |
| `pop(list)` | List | Nilai terakhir | Menghapus dan mengembalikan elemen terakhir list (`terakhir = pop(buah)`) |
| `case_sensitive(a, b)` | Dua teks string | `bool` | Membandingkan dua string dengan membedakan huruf kapital (*case-sensitive*) |
| `incase_sensitive(a, b)` | Dua teks string | `bool` | Membandingkan dua string dengan menyamakan huruf kapital (*case-insensitive*) |
| `lower(str)` | String | `string` | Mengubah seluruh karakter teks menjadi huruf kecil (*lowercase*) |
| `upper(str)` | String | `string` | Mengubah seluruh karakter teks menjadi huruf besar kapital (*uppercase*) |
| `input(prompt)` | String prompt opsional | `string` | Membaca input teks interaktif dari terminal pengguna (membutuhkan `use io`) |
| `ask(prompt)` | String prompt opsional | `string` | Sinonim dari `input` untuk meminta respon teks pengguna (membutuhkan `use io`) |

### 4.3 Kamus Pustaka Standar Bawaan (Standard Modules)

| Pustaka | Cara Memuat | Fitur & Fungsi Utama | Keterangan |
| :--- | :--- | :--- | :--- |
| **`io`** | `use io` atau `import io` | `io.input`, `io.ask`, `io.print`, `io.read`, `io.write` | Wajib dimuat untuk operasi berkas dan input keyboard pengguna |
| **`math`** | `use math` atau `import math` | `math.sqrt`, `math.pow`, `math.abs`, `math.floor`, `math.ceil`, `math.round`, `math.min`, `math.max`, `math.random`, `math.random_seed`, `math.sin`, `math.cos`, `math.tan`, `math.pi`, `math.e` | Perhitungan matematis, trigonometri, dan generator angka acak. Mendukung penetapan seed manual (`math.random_seed(seed)`) untuk hasil deterministik atau reset ke entropi hardware murni (`math.random_seed()`). |
| **`time`** | `use time` atau `import time` | `time.now`, `time.sleep` | Pengukuran waktu (milidetik) dan jeda eksekusi program |
| **`net`** / **`http`** | `use net` atau `import net` | `net.get`, `net.send`, `http.get`, `http.send` | Komunikasi jaringan dan request HTTP |
| **`gui`** | `use gui` atau `import gui` | `gui.app`, `gui.window`, `gui.run` | Aplikasi GUI jendela desktop native Win32 |
| **`str`** | `use str` atau `import str` | `str.lower`, `str.upper`, `str.case_sensitive`, `str.incase_sensitive` | Modul pembantu pemrosesan string dan manipulasi teks |

### 4.4 Kamus Operator, Simbol & Komentar

| Simbol / Operator | Kategori | Penjelasan Singkat | Contoh Kode |
| :--- | :--- | :--- | :--- |
| `#` | Komentar | Menandai baris komentar satu baris (gaya Python/Shell/Ruby) | `# Ini komentar` |
| `//` | Komentar | Menandai baris komentar satu baris (gaya C/C++/JavaScript) | `// Ini komentar` |
| `/* ... */` | Komentar | Menandai blok komentar multi-baris (gaya C/C++/Java) | `/* Blok komentar */` |
| `--` | Komentar | Menandai baris komentar satu baris (gaya Lua) | `-- Komentar Lua` |
| `--[[ ... ]]` | Komentar | Menandai blok komentar multi-baris (gaya Lua) | `--[[ Blok komentar Lua ]]` |
| `'...'`, `"..."` | String Literal | Mengapit teks string literal (kutip tunggal maupun ganda dengan interpolasi `${var}`) | `'Halo'`, `"Umur: ${age}"` |
| `;` | Pemisah Perintah | Memisahkan banyak statement/perintah dalam satu baris yang sama | `x = 10; y = 20; z = x + y;` |
| `=` | Penetapan Nilai | Menyimpan hasil evaluasi ekspresi ke variabel | `x = 10` |
| `.` | Member Access | Mengakses properti objek atau fungsi pustaka | `io.write`, `person.role` |
| `+` | Penjumlahan / Concat | Menjumlahkan dua angka atau menggabungkan string | `10 + 20`, `"A" + "B"` |
| `-` | Pengurangan / Negasi | Mengurangi nilai atau memberikan tanda negatif | `50 - 20`, `-x` |
| `*` | Perkalian | Mengalikan nilai numerik | `6 * 7` |
| `/` | Pembagian | Membagi dua angka secara presisi | `100 / 4` |
| `%` | Modulo | Menghitung sisa hasil bagi | `10 % 3` |
| `==`, `!=`, `~=` | Kesetaraan | Memeriksa kesamaan atau perbedaan dua nilai (`~=` adalah sinonim `!=` gaya Lua) | `a == b`, `x != y`, `x ~= y` |
| `<`, `>`, `<=`, `>=` | Relasional | Membandingkan besar-kecil nilai numerik | `score >= 75` |
| `and`, `or`, `not`, `&&`, `\|\|`, `!` | Logika Boolean | Operator logika AND, OR, dan NOT (kata kunci atau simbol) | `if a and not b`, `x && !y` |
| `[]` | Indexer / Array Literal | Membuat list literal atau mengakses elemen via indeks | `arr = [1, 2]`, `arr[0]` |
| `()` | Prioritas / Panggilan | Mengatur prioritas ekspresi atau memanggil fungsi | `(a + b) * c`, `add(1, 2)` |

---

## 5. Sistem Diagnostik Cerdas & Rekomendasi Typo

Fasthon dilengkapi dengan **Mesin Diagnostik Kontekstual Modern** yang ramah developer (*developer-friendly*), terinspirasi dari gaya diagnostik Python 3.11+ dan Rust, namun dirancang khusus khas Fasthon.

### Fitur Utama Diagnostik:
- **Tampilan Visual Presisi**: Menampilkan nama file, nomor baris, nomor kolom, kutipan baris kode sumber, dan penunjuk caret (`^^^^`) tepat pada token yang bermasalah.
- **Deteksi Typo Cerdas (Fuzzy Suggestion)**: Menggunakan algoritma *Damerau-Levenshtein Distance* untuk mengenali salah ketik (typo) pada:
  - **Keyword**: Misalnya `whlie` disarankan menjadi `while`, `elsif` disarankan menjadi `elseif`/`elif`, `pirnt` disarankan menjadi `print`.
  - **Variabel dalam Scope**: Jika Anda salah mengetik variabel misalnya `tebakn`, mesin otomatis menganalisis variabel yang telah didefinisikan sebelumnya dan menyarankan `tebakan`.
  - **Fungsi & Properti Modul**: Misalnya `math.sqr` disarankan menjadi `math.sqrt`.
- **Rekomendasi Modul Otomatis (`ImportError`)**: Ketika Anda memanggil fungsi modul tanpa `use` terlebih dahulu (misal `input()` atau `math.random()`), mesin tidak hanya memberitahu error melainkan menyertakan solusi instan: `💡 Rekomendasi: use io` beserta baris yang perlu ditambahkan.
- **Zero-Cost Overhead**: Seluruh logika pencarian kemiripan kata dan pemformatan teks hanya berjalan saat terjadi kesalahan (*cold path*). Saat kode berjalan normal, **performa eksekusi Fasthon tetap 100% instan dan tidak terbebani sama sekali**.

#### Contoh Tampilan Diagnostik:
```
File "game.fsn", line 8, col 1
   8 | whlie selesai == false
     | ^^^^^
SyntaxError: Keyword 'whlie' tidak dikenali
  💡 Rekomendasi: Apakah maksud Anda 'while'?

File "game.fsn", line 12, col 4
  12 | if tebakn < angka
     |    ^^^^^^
NameError: Variabel 'tebakn' belum didefinisikan.
  💡 Rekomendasi: Apakah maksud Anda variabel 'tebakan'?

File "skrip.fsn", line 1, col 5
   1 | x = input("Masukkan angka: ")
     |     ^^^^^
ImportError: Modul 'io' belum dimuat. 'input' memerlukan modul 'io'.
  💡 Rekomendasi: use io
  💡 Solusi: Tambahkan perintah 'use io' di bagian atas skrip Anda.
```

---

## 6. Hasil Benchmark & Komparasi Kecepatan

Pengujian performa dilakukan secara langsung di lingkungan Windows 64-bit pada prosesor multi-core dengan membandingkan **FasthonJIT Ultra JIT Engine**, **Fasthon VM (Tanpa JIT)**, **Fasthon Standalone Native Binary**, dan **Python 3.14**.

### Benchmark A: Suite Gabungan (2.000.000 Iterasi Loop + Fibonacci Rekursif $N=28$)

| Runtime / Engine | Waktu Eksekusi | Kecepatan Relatif vs Python |
| :--- | :--- | :--- |
| **FasthonJIT Ultra JIT Engine** | **69 ms** | **10.78x lebih cepat** |
| **Fasthon VM (Tanpa JIT)** | **88 ms** | **8.45x lebih cepat** |
| **Python 3.14** | **744 ms** | Baseline CPython standar |

### Benchmark B: Rekursif Fibonacci Mendalam ($N = 32$)

Menguji performa evaluasi rekursi bertingkat tinggi (4.356.617 pemanggilan fungsi) dengan arsitektur direct JIT machine code call:

| Runtime / Engine | Waktu Eksekusi | Kecepatan Relatif vs Python |
| :--- | :--- | :--- |
| **FasthonJIT Ultra JIT Engine** | **182 ms** | **6.62x lebih cepat** |
| **Python 3.14** | **1.206 ms** | Baseline CPython standar |
| **Fasthon VM (Tanpa JIT)** | **1.440 ms** | Interpretasi Bytecode VM |

### Benchmark C: Loop 10.000.000 Iterasi (Komputasi Intensif)

Menguji performa operasi perulangan dan aritmatika intensif berskala besar:

| Runtime / Engine | Waktu Eksekusi | Kecepatan Relatif vs Python |
| :--- | :--- | :--- |
| **Fasthon Standalone Native Binary (`fasthonjit build`)** | **57 ms** | **37.8x lebih cepat** |
| **FasthonJIT Ultra JIT Engine (`fasthonjit script.fsn`)** | **96 ms** | **22.5x lebih cepat** |
| **Fasthon VM (Tanpa JIT)** | **1.051 ms** | Interpretasi Bytecode VM |
| **Python 3.14** | **2.157 ms** | Baseline CPython standar |

### Benchmark D: Penulisan File I/O Baris Banyak (100.000 Baris Teks / 6.2 MB)

Menguji performa penulisan berkas teks berskala besar menggunakan buffered stream writer (`io.open` + `f.writeline`):

| Runtime / Engine | Waktu Eksekusi | Status & Catatan |
| :--- | :--- | :--- |
| **Fasthon Standalone Native Binary (`fasthonjit build`)** | **92 ms** | 100.000 baris (6.2 MB) ditulis langsung ke disk |
| **FasthonJIT Ultra JIT Engine (`fasthonjit script.fsn`)** | **868 ms** | Eksekusi instan di memori tanpa kompilasi binary |
| **Batch Fast Write (`io.write_lines` 50.000 baris)** | **8 ms** | Satu proses batch writing direct C-level buffer |

---

## 7. Panduan Eksekusi & Kompilasi AOT (Linux, Android Termux, Windows)

### 1. Eksekusi Skrip Instan Berkecepatan Penuh JIT (Default CLI Mode)
Jalankan berkas skrip `.fsn` secara langsung. Mesin JIT x86_64 otomatis mengompilasi fungsi ke instruksi mesin murni di memori eksekusi (*zero disk cache*):

```bash
# Di Linux & Android (Termux):
fasthonjit script.fsn

# Di Windows:
.\fasthonjit.exe script.fsn
```

> 💡 **Opsi Flag JIT**:
> - `fasthonjit --jit-info` : Menampilkan arsitektur CPU target dan status JIT engine.
> - `fasthonjit --jit script.fsn` : Memaksa JIT compiler aktif (default).
> - `fasthonjit --no-jit script.fsn` : Menonaktifkan JIT dan menjalankan interpretasi VM biasa.
> - *Alias*: Perintah `fasthon` dan `eas` tetap tersedia sebagai alias penuh untuk kemudahan transisi.

### 2. Kompilasi AOT ke Executable Mandiri Multi-Platform (`fasthonjit build`)
Perintah `fasthonjit build` mengompilasi skrip `.fsn` menjadi binary executable native mandiri (*self-contained*) dengan optimasi C++20 `-O3`.

Binary yang dihasilkan **100% mandiri** (runtime standard library disematkan langsung di dalam berkas hasil kompilasi), sehingga berkas `.exe` atau binary Linux/Android dapat disalin dan dijalankan di komputer atau perangkat mana saja tanpa memerlukan folder source code FasthonJIT.

#### 🐧 Di Linux:
Pastikan Anda memiliki compiler C++ (`g++` atau `clang++`):
```bash
# Install compiler jika belum ada (Ubuntu/Debian)
sudo apt update && sudo apt install g++ -y

# Kompilasi skrip FasthonJIT
fasthonjit build game.fsn -o game

# Jalankan langsung
./game
```

#### 📱 Di Android (Termux):
`fasthonjit build` dapat berjalan langsung di ponsel Android via Termux menggunakan compiler Clang bawaan Termux:
```bash
# Siapkan Clang di Termux (sekali saja)
pkg update && pkg install clang -y

# Kompilasi skrip FasthonJIT menjadi binary native Android
fasthonjit build game.fsn -o game

# Jalankan langsung di Termux
./game
```

#### 🪟 Di Windows:
Menggunakan compiler MinGW-w64 (`g++` atau `clang++`):
```cmd
fasthonjit build game.fsn -o game.exe
.\game.exe
```

#### 🌐 Target Selection & Cross-Compilation (`--target`):
Anda dapat secara eksplisit memilih target platform binary yang ingin dihasilkan menggunakan opsi `--target`:

```bash
# Build untuk Linux (ELF Binary):
fasthonjit build game.fsn --target linux -o game

# Build untuk Android ARM64 (ELF Binary):
fasthonjit build game.fsn --target android -o game

# Build untuk Windows (.exe):
fasthonjit build game.fsn --target windows -o game.exe
```
> 💡 Anda juga dapat menggunakan flag shortcut seperti `--linux`, `--android`, atau `--windows`.

### 3. Mode Interaktif (Interactive REPL)
Jalankan `fasthonjit` tanpa argumen untuk masuk ke interactive shell:

```bash
# Di Linux & Android (Termux):
fasthonjit

# Di Windows:
.\fasthonjit.exe
```
Ketik `exit` untuk keluar dari REPL.

---

## 8. Lisensi

Proyek ini dirilis di bawah lisensi terbuka **Mozilla Public License 2.0 (MPL-2.0)**. Lihat berkas [`LICENSE`](LICENSE) untuk informasi lebih lanjut.

