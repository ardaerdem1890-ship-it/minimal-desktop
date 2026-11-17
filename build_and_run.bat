@echo off
rem Minimal Desktop - one-click build + test + run
rem   1. builds build/minimal_desktop.iso
rem   2. runs the headless autotest (exits 0 on PASS, 1 on FAIL)
rem   3. launches QEMU with -serial stdio -m 256M

cd /d "%~dp0.."

echo [1/3] Building kernel and ISO
make -j4

echo [2/3] Running headless autotest
make autotest

echo [3/3] Launching QEMU
make run
