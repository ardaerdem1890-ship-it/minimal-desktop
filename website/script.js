/* Minimal Desktop landing page — boot animation + interactive shell demo */
(function () {
  "use strict";

  const $ = (sel, root) => (root || document).querySelector(sel);
  const reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* =========================================================
     Boot animation (hero)
     ========================================================= */
  const bootScreen = $("#boot-screen");
  const bootLines = [
    { t: "GRUB 2.12  Multiboot2  ->  /boot/kernel.elf", c: "dim" },
    { t: "Minimal Desktop 1.4.1 (mdos-i386)  [Nova+]", c: "hi" },
    { t: "memory: 256 MiB  |  cmdline: --autotest", c: "dim" },
    { t: "[ OK ] serial COM1 @ 115200 8N1", c: "" },
    { t: "[ OK ] VGA text mode 80x25, 16 colours", c: "" },
    { t: "[ OK ] GDT + IDT + PIC remapped", c: "" },
    { t: "[ OK ] PIT channel 0 @ 100 Hz", c: "" },
    { t: "[ OK ] PS/2 keyboard IRQ1, mouse IRQ12", c: "" },
    { t: "[ OK ] RAMFS mounted, 9 nodes", c: "" },
    { t: "[ OK ] store archive: 3 packages", c: "" },
    { t: "[ OK ] PCI: RTL8139 / PCnet / e1000 NIC", c: "cyan" },
    { t: "[ OK ] DHCP lease 10.0.2.15/24 gw 10.0.2.2", c: "cyan" },
    { t: "[ OK ] DNS 10.0.2.3  link UP 100 Mbit", c: "cyan" },
    { t: "", c: "" },
    { t: "Minimal Desktop 1.4.1  —  type 'help' to begin", c: "amber" },
    { t: "minimal@os:~$ ", c: "prompt" }
  ];

  function bootAnimate() {
    if (!bootScreen) return;
    if (reduced) {
      bootLines.forEach((l) => addBootLine(l.t, l.c));
      return;
    }
    let i = 0;
    const tick = () => {
      if (i >= bootLines.length) return;
      const l = bootLines[i++];
      addBootLine(l.t, l.c);
      bootScreen.scrollTop = bootScreen.scrollHeight;
      setTimeout(tick, l.t ? 95 + Math.random() * 90 : 60);
    };
    setTimeout(tick, 350);
  }

  function addBootLine(text, cls) {
    const span = document.createElement("span");
    span.className = cls || "";
    span.textContent = text + "\n";
    bootScreen.appendChild(span);
  }

  bootAnimate();

  /* =========================================================
     Interactive terminal demo
     ========================================================= */
  const screen = $("#demo-screen");
  const form = $("#demo-form");
  const input = $("#demo-cmd");
  const promptLabel = $("#demo-prompt");
  const clearBtn = $("#demo-clear");

  const state = {
    cwd: "/home/minimal",
    installed: new Set(),
    theme: "green",
    user: "minimal",
    host: "os"
  };

  const COMMANDS = [
    "help", "sysinfo", "uname", "uptime", "drivers", "taskmng", "selftest",
    "clear", "date", "time", "ls", "cd", "pwd", "mkdir", "touch", "rm",
    "cat", "write", "echo", "memmap", "hexdump", "df", "calc", "theme",
    "matrix", "beep", "credits", "snake", "pong", "startgui", "desktop",
    "store", "ifconfig", "ping", "nslookup", "curl", "netstat", "neofetch",
    "whoami", "reboot", "shutdown"
  ];

  const history = [];
  let historyIndex = 0;

  function promptText() {
    const p = state.cwd === "/home/minimal" ? "~" : state.cwd;
    return `${state.user}@${state.host}:${p}$`;
  }

  function updatePrompt() {
    if (promptLabel) promptLabel.textContent = promptText();
  }

  function scrollBottom() {
    if (screen) screen.scrollTop = screen.scrollHeight;
  }

  function print(text, cls) {
    if (!screen) return;
    const parts = String(text).split("\n");
    parts.forEach((line) => {
      const div = document.createElement("div");
      div.className = "line" + (cls ? " " + cls : "");
      div.textContent = line.length ? line : "\u00a0";
      screen.appendChild(div);
    });
    scrollBottom();
  }

  function printPromptLine(cmd) {
    if (!screen) return;
    const div = document.createElement("div");
    div.className = "line promptline";
    const b = document.createElement("b");
    b.textContent = promptText() + " ";
    div.appendChild(b);
    div.appendChild(document.createTextNode(cmd));
    screen.appendChild(div);
    scrollBottom();
  }

  function clearScreen() {
    if (screen) screen.innerHTML = "";
    print("Minimal Desktop 1.0.0 — browser demo (not the real kernel)", "dim");
    print("Type 'help' for commands. ↑/↓ history, Tab completion.", "dim");
    print("");
  }

  const commandList = {
    help() {
      print("Minimal Desktop shell — command reference", "cyan");
      print("");
      print("SYSTEM", "amber");
      print("  help                 this list (help <command> for details)");
      print("  sysinfo              CPU, RAM, OS version, uptime");
      print("  uname                kernel name and architecture");
      print("  uptime               time since boot and PIT ticks");
      print("  drivers              loaded drivers, IRQs and status");
      print("  taskmng              live task table (simulated)");
      print("  selftest             run 13 kernel/subsystem checks");
      print("  date / time          CMOS real-time clock");
      print("  clear                clear the console");
      print("  reboot / shutdown    reset or ACPI power-off");
      print("");
      print("FILES & MEMORY", "amber");
      print("  ls [path]            list directory");
      print("  cd <path>            change directory");
      print("  pwd                  print working directory");
      print("  mkdir <dir>          create directory");
      print("  touch <file>         create empty file");
      print("  cat <file>           print file");
      print("  write <file> <text>  overwrite file with text");
      print("  echo <text>          print text (supports > file)");
      print("  rm <path>            remove file or directory");
      print("  hexdump <file>       hex + ASCII dump");
      print("  memmap               E820 map, frames and heap");
      print("  df                   RAMFS usage");
      print("");
      print("NETWORK", "amber");
      print("  ifconfig             interface, IP, MAC, counters");
      print("  ping <ip>            ICMP echo request");
      print("  nslookup <name>      DNS A-record lookup");
      print("  curl <url>           HTTP/1.0 GET over TCP");
      print("  netstat              ARP cache and socket table");
      print("");
      print("STORE & DESKTOP", "amber");
      print("  store list           available packages");
      print("  store install <pkg>  snake | pong | minimal-desktop");
      print("  store remove <pkg>   uninstall");
      print("  startgui             launch Minimal Desktop (mode 13h)");
      print("");
      print("UTILITIES", "amber");
      print("  calc <expr>          integer expression evaluator");
      print("  theme <name>         green | amber | blue | mono");
      print("  matrix               digital rain (simulated)");
      print("  beep [freq] [ms]     PC speaker");
      print("  credits              about the project");
    },

    sysinfo() {
      print("         _ _ _  Minimal Desktop 1.0.0 \"Retro\"", "cyan");
      print("  OS      : Minimal Desktop (mdos-i386)");
      print("  Kernel  : 1.0.0  built " + new Date().toISOString().slice(0, 10));
      print("  Arch    : x86, 32-bit protected mode, ring 0");
      print("  CPU     : QEMU Virtual CPU (CPUID vendor: GenuineIntel)");
      print("  Memory  : 256 MiB total, 241 MiB usable");
      print("  Frames  : 65536 x 4 KiB, 61440 free");
      print("  Heap    : 4 MiB kernel heap, 37 KiB used");
      print("  Console : VGA text 80x25 + COM1 serial mirror");
      print("  Uptime  : " + uptimeString());
      print("  Tasks   : 8 registered");
      print("  Network : eth0 10.0.2.15 (DHCP), link UP");
      print("  Store   : " + state.installed.size + " installed / 3 available");
    },

    uname() {
      print("Minimal Desktop mdos 1.0.0 i386 protected-mode");
    },

    uptime() {
      print(`up ${uptimeString()}, 1 user, load average: 0.00, 0.01, 0.00`);
      print("PIT: 100 Hz, " + (uptimeSeconds() * 100).toLocaleString() + " ticks");
    },

    drivers() {
      print("DRIVER       IRQ   I/O BASE   STATUS");
      print("--------------------------------------------------------");
      print("vga-text      -    0xB8000    OK   80x25, 16 colours");
      print("vga-gfx       -    0xA0000    OK   mode 13h, 320x200x256");
      print("serial-com1   4    0x3F8      OK   115200 8N1, mirror on");
      print("pit           0    0x40       OK   100 Hz system tick");
      print("ps2-keyboard  1    0x60/0x64  OK   IRQ-driven, set-1");
      print("ps2-mouse     12   0x60/0x64  OK   3-byte packets");
      print("ata-ide       14   0x1F0      OK   PIO, primary master");
      print("ramfs         -    -          OK   in-memory filesystem");
      print("pci           -    -          OK   1 device enumerated");
      print("rtl8139       11   0xC000     OK   52:54:00:12:34:56, link UP");
      print("net-stack     -    -          OK   eth/arp/ipv4/icmp/udp/dhcp/dns/tcp");
      print("ps2-aux       12   -          OK   mouse present");
    },

    taskmng() {
      print(" PID  NAME                 STATE      MEM     TICKS   TYPE");
      print(" ---  -------------------  ---------  ------  ------  --------");
      print(" 1    kernel               running     1024K   18402   kernel");
      print(" 2    shell                running      128K    9031   kernel");
      print(" 3    vga-console          ready         64K     241   kernel");
      print(" 4    serial-com1          ready         32K     102   kernel");
      print(" 5    rtl8139-rx           ready        256K    1180   kernel");
      print(" 6    dhcp-client          ready         48K     201   kernel");
      print(" 7    ps2-input            ready         16K     844   kernel");
      print(" 8    store-daemon         ready         96K      77   kernel");
      print("");
      print("tip: in the real shell, press k then a PID to kill a module task.", "dim");
    },

    selftest() {
      const tests = [
        "Multiboot2 information parsed",
        "GDT loaded (code 0x08, data 0x10)",
        "IDT loaded, PIC remapped to 0x20",
        "Paging enabled, 1 GiB identity map",
        "PIT tick increments at 100 Hz",
        "Physical frame allocator alloc/free",
        "Kernel heap alloc/write/read/free",
        "RAMFS create/write/read/unlink",
        "Command parser dispatch",
        "Store manifest: snake/pong/minimal-desktop",
        "Keyboard controller + IRQ1",
        "PS/2 mouse IRQ12",
        "RTL8139 link + DHCP lease",
        "ICMP echo to gateway"
      ];
      print("Running kernel self-test suite...", "dim");
      tests.forEach((t) => print("[PASS] " + t, "ok"));
      print("");
      print(`RESULT: ${tests.length} passed, 0 failed, 0 skipped`, "ok");
      print("ALL TESTS PASSED", "ok");
      print("(browser simulation — the real suite prints to VGA and COM1)", "dim");
    },

    date() {
      const d = new Date();
      print(d.toDateString() + " " + d.toTimeString().slice(0, 8) + " " + d.getFullYear());
      print("RTC: CMOS 0x70/0x71, BCD decoded, 24-hour mode", "dim");
    },

    time() {
      print(new Date().toTimeString().slice(0, 8));
    },

    clear() {
      clearScreen();
    },

    ls(args) {
      const path = args[0] || state.cwd;
      if (path === "/" || path === "~" || path === "/home/minimal") {
        if (path === "/") {
          print("readme.txt           home/   usr/   etc/   tmp/");
        } else {
          print("welcome.txt         448 B");
          print("notes.txt           207 B");
          print("README.txt           37 B");
        }
        return;
      }
      if (path === "/usr" || path === "/usr/bin" || path === "/usr/packages") {
        if (path === "/usr/packages" && state.installed.size) {
          state.installed.forEach((n) => print(`${n}.mdp  installed`));
        } else {
          print("(empty)");
        }
        return;
      }
      if (path === "/etc") { print("hostname   motd"); return; }
      if (path === "/tmp") { print("(empty)"); return; }
      print(`ls: ${path}: no such directory`, "err");
    },

    cd(args) {
      const target = args[0];
      if (!target || target === "~" || target === "/home/minimal") { state.cwd = "/home/minimal"; updatePrompt(); return; }
      if (target === "/" || target === "/usr" || target === "/usr/bin" || target === "/usr/packages" || target === "/etc" || target === "/tmp") {
        state.cwd = target; updatePrompt(); return;
      }
      if (target === "..") {
        state.cwd = state.cwd === "/home/minimal" ? "/home" : "/home/minimal";
        updatePrompt(); return;
      }
      print(`cd: ${target}: no such directory`, "err");
    },

    pwd() { print(state.cwd); },

    mkdir(args) {
      if (!args[0]) { print("usage: mkdir <path>", "err"); return; }
      print(`mkdir: created directory '${args[0]}'`);
    },

    touch(args) {
      if (!args[0]) { print("usage: touch <path>", "err"); return; }
      print(`touch: created '${args[0]}' (0 bytes)`);
    },

    rm(args) {
      if (!args[0]) { print("usage: rm <path>", "err"); return; }
      print(`rm: removed '${args[0]}'`);
    },

    cat(args) {
      const f = (args[0] || "").replace(/^.*\//, "");
      const files = {
        "welcome.txt": "Welcome to Minimal Desktop!\n\nThis desktop and shell are running directly on bare metal\nwith no underlying operating system.\n\nTry: store install minimal-desktop, then startgui",
        "notes.txt": "Retro notes\n-----------\n- VGA text: 80x25, 16 colours\n- GUI: VGA mode 13h\n- PIT: 100 Hz\n- Packages: FNV-1a verified",
        "readme.txt": "Minimal Desktop 1.0.0\nType 'help' for the command list.",
        "README.txt": "Home directory of user minimal.",
        "hostname": "minimal",
        "motd": "minimal@os - boot fast, stay retro."
      };
      if (files[f]) print(files[f]);
      else print(`cat: ${args[0] || "(none)"}: no such file`, "err");
    },

    write(args) {
      if (args.length < 2) { print("usage: write <file> <text...>", "err"); return; }
      const text = args.slice(1).join(" ");
      print(`write: ${args[0]} <- ${text.length} bytes`);
    },

    echo(args) {
      const gt = args.indexOf(">");
      if (gt >= 0) {
        print(`echo: wrote ${args.slice(gt + 1).join(" ").length} bytes to ${args[gt + 1] || "?"}`);
        return;
      }
      print(args.join(" "));
    },

    memmap() {
      print("Physical memory map (Multiboot2 E820):");
      print("  #  base          length        type");
      print("  0  00000000      0009FC00      available");
      print("  1  0009FC00      00000400      reserved");
      print("  2  000F0000      00010000      reserved");
      print("  3  00100000      0FE00000      available");
      print("  4  FFFC0000      00040000      reserved");
      print("");
      print("Allocator state:");
      print("  physical RAM  : 256 MiB (65536 frames of 4096 bytes)");
      print("  free frames   : 61440 (240 MiB)");
      print("  kernel heap   : 37 / 4096 KiB used, base 0x00400000");
      print("  paging        : enabled, page dir @ 0x003FF000, 1 GiB mapped");
    },

    hexdump(args) {
      if (!args[0]) { print("usage: hexdump <file>", "err"); return; }
      const f = args[0].replace(/^.*\//, "");
      if (f !== "welcome.txt" && f !== "notes.txt") { print(`hexdump: ${args[0]}: no such file`, "err"); return; }
      print("00000000  57 65 6c 63 6f 6d 65 20  74 6f 20 4d 69 6e 69 6d  |Welcome to Minim|");
      print("00000010  61 6c 20 44 65 73 6b 74  6f 70 21 0a 0a 54 68 69  |al Desktop!..Thi|");
      print("00000020  73 20 64 65 73 6b 74 6f  70 20 61 6e 64 20 73 68  |s desktop and sh|");
      print("...");
    },

    df() {
      print("Filesystem      Size    Used    Avail   Use%   Mounted on");
      print("ramfs           4.0M    12K     4.0M    0%     /");
      print("store-initrd    12K     12K     0       100%   /usr/packages");
    },

    calc(args) {
      const expr = args.join(" ");
      if (!expr) { print("usage: calc <expression>   e.g. calc (2+3)*7"); return; }
      try {
        const value = evaluate(expr);
        if (!Number.isFinite(value)) throw new Error("bad expression");
        print(`${expr} = ${value}`);
      } catch (e) {
        print(`calc: ${expr}: syntax error`, "err");
      }
    },

    theme(args) {
      const name = (args[0] || "").toLowerCase();
      const themes = {
        green: "#49f08a", amber: "#ffc857", blue: "#4bd6ef", mono: "#d8e6e2"
      };
      if (!name) { print(`theme: current=${state.theme}, available: green amber blue mono`); return; }
      if (!themes[name]) { print(`theme: unknown '${name}' (green|amber|blue|mono)`, "err"); return; }
      state.theme = name;
      if (screen) screen.style.color = themes[name];
      print(`theme: applied '${name}' to VGA foreground`);
    },

    matrix() {
      print("digital rain — press ESC to stop (simulated)", "dim");
      const chars = "01アイウエオカキクケコサシスセソタチツテトナニヌネノ";
      for (let i = 0; i < 6; i++) {
        print(Array.from({ length: 62 }, () => chars[Math.floor(Math.random() * chars.length)]).join(""), "ok");
      }
      print("(matrix would animate until ESC in the real console)", "dim");
    },

    beep(args) {
      const freq = parseInt(args[0], 10) || 880;
      const ms = parseInt(args[1], 10) || 120;
      print(`beep: PIT channel 2, ${freq} Hz for ${ms} ms (PC speaker)`);
    },

    credits() {
      print("Minimal Desktop 1.0.0 \"Retro\"", "cyan");
      print("A complete x86 hobby operating system built from scratch.");
      print("");
      print("Kernel & boot   : C + NASM, Multiboot2, GDT/IDT/PIC");
      print("Drivers         : VGA, COM1, PIT, PS/2, ATA, RTL8139, PCI");
      print("Filesystem      : RAMFS (volatile), store initrd");
      print("Network stack   : Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP");
      print("GUI             : VGA mode 13h compositor at 320x200");
      print("");
      print("Built with i686-elf-gcc, NASM, GRUB, xorriso and QEMU.");
    },

    store(args) {
      const sub = (args[0] || "").toLowerCase();
      const pkgs = {
        snake: { v: "1.0.0", size: 2184, cat: "games", desc: "Terminal snake game" },
        pong: { v: "1.0.0", size: 2312, cat: "games", desc: "Pong with AI opponent" },
        "minimal-desktop": { v: "1.0.0", size: 3456, cat: "desktop", desc: "Graphical desktop environment" }
      };

      if (!sub || sub === "list" || sub === "ls") {
        print("Minimal Store - 3 packages, repository: http://10.0.2.2:8000");
        print("  S  PACKAGE           VERSION   SIZE      CATEGORY   DESCRIPTION");
        print("  -  ----------------  --------  --------  ---------  --------------------");
        for (const [name, p] of Object.entries(pkgs)) {
          const mark = state.installed.has(name) ? "*" : " ";
          print(`  ${mark}  ${name.padEnd(16)}  ${p.v.padEnd(8)}  ${String(p.size + " B").padStart(8)}  ${p.cat.padEnd(9)}  ${p.desc}`);
        }
        print("  (* = installed)  use: store install <package>");
        return;
      }

      if (sub === "install" || sub === "fetch") {
        const name = (args[1] || "").toLowerCase();
        if (!pkgs[name]) { print(`store: package '${args[1] || ""}' not found. Try 'store list'.`, "err"); return; }
        if (state.installed.has(name)) { print(`store: ${name} is already installed`); return; }
        print(`Installing ${name} ${pkgs[name].v} (${pkgs[name].size} bytes, http)`);
        print("  [net] GET http://10.0.2.2:8000/" + name + ".mdp");
        print("  unpacking         [##################] 100%");
        print("  [PASS] checksum verified: " + randomChecksum());
        state.installed.add(name);
        print(`  [ OK ] registered module #${name === "snake" ? 1 : name === "pong" ? 2 : 3}, unpacked to /usr/packages/${name}.mdp`);
        if (name === "snake" || name === "pong") print(`  Run it with: ${name}`);
        else print("  Run it with: startgui");
        return;
      }

      if (sub === "remove" || sub === "uninstall") {
        const name = (args[1] || "").toLowerCase();
        if (!state.installed.has(name)) { print(`store: ${name || "?"} is not installed`, "err"); return; }
        state.installed.delete(name);
        print(`store: removed ${name}`);
        return;
      }

      if (sub === "info") {
        const name = (args[1] || "").toLowerCase();
        const p = pkgs[name];
        if (!p) { print(`store: package '${name}' not found`, "err"); return; }
        print(`Package : ${name} ${p.v}`);
        print(`Category: ${p.cat}`);
        print(`Size    : ${p.size} bytes`);
        print("Checksum: " + randomChecksum() + " (FNV-1a)");
        print(`Status  : ${state.installed.has(name) ? "installed" : "available"}`);
        print(`About   : ${p.desc}`);
        return;
      }

      if (sub === "repo") { print("repository: http://10.0.2.2:8000"); return; }
      print(`store: unknown subcommand '${args[0]}'`, "err");
    },

    ifconfig() {
      print("eth0: RTL8139  flags=UP,BROADCAST,RUNNING  mtu 1500");
      print("      ether 52:54:00:12:34:56");
      print("      inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255");
      print("      gateway 10.0.2.2  dns 10.0.2.3  DHCP lease 86400s");
      print("      RX packets 42  bytes 6120   TX packets 18  bytes 2048");
      print("      IRQ 11  I/O 0xC000  link UP (100 Mbit full-duplex)");
      print("      ARP cache: 10.0.2.2 -> 52:55:0a:00:02:02");
    },

    ping(args) {
      const ip = args[0] || "10.0.2.2";
      if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip)) { print(`ping: ${ip}: invalid address`, "err"); return; }
      print(`PING ${ip} (${ip}): 56 data bytes`);
      const good = ip === "10.0.2.2" || ip === "10.0.2.3" || ip === "10.0.2.15";
      if (!good) {
        print(`ping: ${ip}: network unreachable (only the QEMU 10.0.2.0/24 segment is simulated)`, "err");
        return;
      }
      for (let i = 1; i <= 3; i++) {
        print(`64 bytes from ${ip}: icmp_seq=${i} ttl=64 time=${(0.25 + Math.random() * 0.4).toFixed(1)} ms`);
      }
      print(`--- ${ip} ping statistics ---`);
      print("3 packets transmitted, 3 received, 0% packet loss");
    },

    nslookup(args) {
      const name = args[0] || "example.com";
      print(`Server:  10.0.2.3`);
      print(`Address: 10.0.2.3#53`);
      print("");
      if (name === "example.com" || name === "www.example.com") {
        print("Non-authoritative answer:");
        print("Name:    example.com");
        print("Address: 93.184.216.34");
      } else if (name === "minimal.os" || name === "minimal-desktop.local") {
        print("Name:    " + name);
        print("Address: 10.0.2.15");
      } else {
        print(`** server can't find ${name}: NXDOMAIN`, "err");
      }
    },

    curl(args) {
      const url = args[0] || "http://example.com/";
      const m = url.match(/^http:\/\/([^/:]+)(?::(\d+))?(\/.*)?$/);
      if (!m) { print("curl: only http:// URLs are supported", "err"); return; }
      const host = m[1];
      const port = m[2] || "80";
      const path = m[3] || "/";
      print(`*   Trying ${host}:${port}...`);
      print(`* Connected. Sending GET ${path} HTTP/1.0`);
      if (host === "example.com" || host === "www.example.com") {
        print("HTTP/1.0 200 OK");
        print("Content-Type: text/html; charset=UTF-8");
        print("Content-Length: 1256");
        print("");
        print("<!doctype html><html><head><title>Example Domain</title>");
        print("<style>body{background:#f0f0f2;font-family:sans-serif}</style>");
        print("</head><body><h1>Example Domain</h1>");
        print("<p>This domain is for use in illustrative examples.</p>");
        print("</body></html>");
        print("");
        print("* Transfer complete: 1256 bytes", "dim");
      } else if (host === "10.0.2.2" || host === "10.0.2.15") {
        print("HTTP/1.0 200 OK");
        print("Server: Minimal-Web/1.0");
        print("Content-Type: text/html");
        print("");
        print("<html><body><h1>Minimal Web</h1><p>Hello from the host!</p></body></html>");
        print("* Transfer complete: 431 bytes", "dim");
      } else {
        print(`* connect to ${host} port ${port}: connection timed out`, "err");
        print("curl: (7) failed to connect", "err");
      }
    },

    netstat() {
      print("ARP cache:");
      print("  10.0.2.2   52:55:0a:00:02:02  eth0  REACHABLE");
      print("  10.0.2.3   52:55:0a:00:02:03  eth0  STALE");
      print("");
      print("Active sockets:");
      print("  Proto  Local            Remote           State");
      print("  tcp    10.0.2.15:49152  93.184.216.34:80 ESTABLISHED");
      print("  udp    0.0.0.0:68       10.0.2.2:67      DHCP");
      print("  udp    10.0.2.15:53001  10.0.2.3:53      DNS");
      print("");
      print("Driver: rx 42 pkts / 6120 B, tx 18 pkts / 2048 B, 0 drops");
    },

    neofetch() {
      print("        ▄▄▄▄▄▄▄        minimal@os", "cyan");
      print("      ▄█████████▄      ----------");
      print("     ███▀     ▀███     OS: Minimal Desktop 1.0.0");
      print("    ███   ▄▄▄   ███    Kernel: mdos-i386 32-bit");
      print("    ███  █████  ███    Shell: msh");
      print("     ███▄     ▄███     Uptime: " + uptimeString());
      print("      ▀█████████▀      Packages: " + state.installed.size + " installed");
      print("        ▀▀▀▀▀▀▀        Network: 10.0.2.15 (RTL8139)");
      print("                       Resolution: 80x25 / 320x200");
      print("                       Theme: phosphor green");
    },

    whoami() { print(state.user); },

    snake() {
      if (!state.installed.has("snake")) { print("snake: module not loaded. Run: store install snake", "err"); return; }
      print("┌────────────────────────────────────────────┐", "ok");
      print("│  SNAKE       score 0        best 0        │", "ok");
      print("│  ●●●●                                      │", "ok");
      print("│            ◆                               │", "ok");
      print("│                                            │", "ok");
      print("└────────────────────────────────────────────┘", "ok");
      print("(simulated — the real game uses PIT ticks and IRQ1)", "dim");
    },

    pong() {
      if (!state.installed.has("pong")) { print("pong: module not loaded. Run: store install pong", "err"); return; }
      print("┌────────────────────────────────────────────┐", "ok");
      print("│  AI 2                    YOU 0             │", "ok");
      print("│  █                                  █      │", "ok");
      print("│                   ●                        │", "ok");
      print("│  █                                  █      │", "ok");
      print("└────────────────────────────────────────────┘", "ok");
      print("(simulated — W/S or arrows control the left paddle)", "dim");
    },

    startgui() {
      if (!state.installed.has("minimal-desktop")) {
        print("startgui: minimal-desktop is not installed.", "err");
        print("Run: store install minimal-desktop", "dim");
        return;
      }
      print("Switching to VGA mode 13h (320x200x256)...");
      print("Minimal Desktop window manager started. ESC returns to console.", "ok");
      setTimeout(() => {
        const el = document.getElementById("desktop");
        if (el) el.scrollIntoView({ behavior: reduced ? "auto" : "smooth" });
      }, 650);
    },

    reboot() {
      print("Rebooting via keyboard controller (0x64 <- 0xFE)...", "amber");
      print("(the real kernel performs a triple-fault fallback)", "dim");
    },

    shutdown() {
      print("ACPI power off: outw(0x604, 0x2000)", "amber");
      print("Goodbye.", "ok");
    }
  };

  /* aliases */
  commandList.desktop = commandList.startgui;

  function evaluate(expr) {
    let pos = 0;
    const s = expr.replace(/\s+/g, "");
    function parseExpr() {
      let v = parseTerm();
      while (pos < s.length && (s[pos] === "+" || s[pos] === "-")) {
        const op = s[pos++];
        const r = parseTerm();
        v = op === "+" ? v + r : v - r;
      }
      return v;
    }
    function parseTerm() {
      let v = parsePower();
      while (pos < s.length && (s[pos] === "*" || s[pos] === "/" || s[pos] === "%")) {
        const op = s[pos++];
        const r = parsePower();
        if ((op === "/" || op === "%") && r === 0) throw new Error("division by zero");
        v = op === "*" ? v * r : op === "/" ? Math.trunc(v / r) : v % r;
      }
      return v;
    }
    function parsePower() {
      let v = parseUnary();
      if (pos < s.length && s[pos] === "^") { pos++; v = Math.pow(v, parsePower()); }
      return v;
    }
    function parseUnary() {
      if (s[pos] === "-") { pos++; return -parseUnary(); }
      if (s[pos] === "+") { pos++; return parseUnary(); }
      return parseAtom();
    }
    function parseAtom() {
      if (s[pos] === "(") {
        pos++;
        const v = parseExpr();
        if (s[pos] !== ")") throw new Error("missing )");
        pos++;
        return v;
      }
      const start = pos;
      if (s.startsWith("0x", pos)) {
        pos += 2;
        while (/[0-9a-fA-F]/.test(s[pos])) pos++;
        if (pos === start + 2) throw new Error("bad hex");
        return parseInt(s.slice(start + 2, pos), 16);
      }
      while (/[0-9]/.test(s[pos])) pos++;
      if (pos === start) throw new Error("expected number");
      return parseInt(s.slice(start, pos), 10);
    }
    const result = parseExpr();
    if (pos !== s.length) throw new Error("trailing input");
    return result;
  }

  function uptimeSeconds() {
    const t = performance.now() / 1000 + 4217;
    return Math.floor(t);
  }
  function uptimeString() {
    const s = uptimeSeconds();
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
    return `${h}h ${m}m ${sec}s`;
  }
  function randomChecksum() {
    return Math.floor(Math.random() * 0xffffffff).toString(16).padStart(8, "0");
  }

  function execute(raw) {
    const line = raw.trim();
    if (!line) { printPromptLine(""); return; }
    printPromptLine(line);
    history.push(line);
    historyIndex = history.length;

    const args = tokenize(line);
    const cmd = args.shift();
    const fn = commandList[cmd];
    if (!fn) {
      print(`msh: command not found: ${cmd}`, "err");
      print("Try 'help' for a list of built-in commands.", "dim");
      return;
    }
    try {
      fn(args);
    } catch (e) {
      print(`msh: ${cmd}: internal demo error`, "err");
    }
  }

  function tokenize(line) {
    const out = [];
    let cur = "";
    let quote = null;
    for (let i = 0; i < line.length; i++) {
      const ch = line[i];
      if (quote) {
        if (ch === quote) quote = null;
        else cur += ch;
      } else if (ch === '"' || ch === "'") {
        quote = ch;
      } else if (/\s/.test(ch)) {
        if (cur) { out.push(cur); cur = ""; }
      } else {
        cur += ch;
      }
    }
    if (cur) out.push(cur);
    return out;
  }

  if (form && input) {
    form.addEventListener("submit", (e) => {
      e.preventDefault();
      const v = input.value;
      input.value = "";
      execute(v);
    });

    input.addEventListener("keydown", (e) => {
      if (e.key === "ArrowUp") {
        e.preventDefault();
        if (!history.length) return;
        historyIndex = Math.max(0, historyIndex - 1);
        input.value = history[historyIndex] || "";
      } else if (e.key === "ArrowDown") {
        e.preventDefault();
        historyIndex = Math.min(history.length, historyIndex + 1);
        input.value = history[historyIndex] || "";
      } else if (e.key === "Tab") {
        e.preventDefault();
        const prefix = input.value.trim();
        if (!prefix) return;
        const hit = COMMANDS.find((c) => c.startsWith(prefix));
        if (hit) input.value = hit + " ";
      } else if (e.key === "l" && e.ctrlKey) {
        e.preventDefault();
        clearScreen();
      } else if (e.key === "Escape") {
        input.value = "";
      }
    });
  }

  if (screen) {
    screen.addEventListener("click", () => input && input.focus());
  }

  if (clearBtn) {
    clearBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      clearScreen();
      input && input.focus();
    });
  }

  document.querySelectorAll("[data-term]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const cmd = btn.getAttribute("data-term");
      execute(cmd);
      input && input.focus();
      const term = document.getElementById("terminal");
      if (term) term.scrollIntoView({ behavior: reduced ? "auto" : "smooth", block: "center" });
    });
  });

  /* boot the simulated shell */
  if (screen) {
    clearScreen();
    print("Minimal Desktop 1.0.0 — interactive browser demo", "cyan");
    print("This simulates msh, the real kernel shell. Try 'help', 'ifconfig',", "dim");
    print("'ping 10.0.2.2', 'curl http://example.com/' or 'store list'.", "dim");
    print("");
  }
  updatePrompt();

  /* =========================================================
     Copy buttons
     ========================================================= */
  document.querySelectorAll(".copy-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const text = btn.getAttribute("data-copy") || "";
      try {
        await navigator.clipboard.writeText(text);
        const old = btn.textContent;
        btn.textContent = "copied";
        setTimeout(() => (btn.textContent = old), 1200);
      } catch (e) {
        btn.textContent = "ctrl+c";
      }
    });
  });

  /* =========================================================
     ISO download availability
     ========================================================= */
  const isoLinks = Array.from(document.querySelectorAll(".btn-download"));
  const isoSizeEls = ["#iso-size", "#iso-size-2", "#iso-size-row"]
    .map((sel) => document.querySelector(sel))
    .filter(Boolean);
  const isoNote = document.getElementById("download-note");
  const isoHint = document.getElementById("download-hint");
  let isoReady = false;

  function formatBytes(n) {
    if (!n) return "ready";
    if (n >= 1024 * 1024) return (n / (1024 * 1024)).toFixed(1) + " MB";
    return Math.max(1, Math.round(n / 1024)) + " KB";
  }

  async function checkIso() {
    try {
      const res = await fetch("downloads/minimal_desktop.iso", { method: "HEAD", cache: "no-store" });
      if (!res.ok) throw new Error("not built");
      const len = Number(res.headers.get("content-length") || 0);
      isoReady = true;
      isoSizeEls.forEach((el) => (el.textContent = formatBytes(len)));
      isoLinks.forEach((a) => a.classList.remove("pending"));
      if (isoNote) isoNote.textContent = "Ready · " + formatBytes(len) + " · GRUB Multiboot2 · QEMU / VirtualBox / USB";
      if (isoHint) {
        isoHint.textContent = "Ready to download. Boot it with: qemu-system-i386 -cdrom minimal_desktop.iso -m 256M";
        isoHint.classList.add("ready");
      }
    } catch (e) {
      isoReady = false;
      isoSizeEls.forEach((el) => (el.textContent = "build pending"));
      isoLinks.forEach((a) => a.classList.add("pending"));
      if (isoNote) isoNote.textContent = "The ISO is produced by the build pipeline — run build_and_run.bat or make";
      if (isoHint) isoHint.innerHTML = 'Awaiting the build pipeline. Run <code>build_and_run.bat</code> to produce it.';
    }
  }

  isoLinks.forEach((a) => {
    a.addEventListener("click", (e) => {
      if (isoReady) return;
      e.preventDefault();
      const build = document.getElementById("build");
      if (build) build.scrollIntoView({ behavior: reduced ? "auto" : "smooth" });
      if (isoHint) isoHint.classList.add("ready");
    });
  });

  checkIso();

  /* =========================================================
     Reveal-on-scroll (progressive enhancement)
     ========================================================= */
  if (!reduced && "IntersectionObserver" in window) {
    const candidates = document.querySelectorAll(
      ".section-head, .card, .pipeline, .gui-features, .boot-flags > div, .qemu-line, .desktop-stage"
    );
    const io = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add("in");
          io.unobserve(entry.target);
        }
      });
    }, { threshold: 0.08, rootMargin: "0px 0px -30px 0px" });

    candidates.forEach((el, i) => {
      el.classList.add("reveal");
      el.style.transitionDelay = Math.min(i % 4, 3) * 55 + "ms";
      io.observe(el);
    });
  }

  /* =========================================================
     Versions list
     ========================================================= */
  async function loadVersions() {
    const container = document.getElementById("versions-list");
    if (!container) return;
    try {
      const res = await fetch("downloads/versions.json", { cache: "no-store" });
      if (!res.ok) throw new Error("versions manifest not found");
      const versions = await res.json();
      if (!Array.isArray(versions) || versions.length === 0) {
        container.innerHTML = '<p class="versions-empty">No versions published yet. Run the build pipeline to produce an ISO.</p>';
        return;
      }
      container.innerHTML = "";
      versions.forEach((v) => {
        const item = document.createElement("article");
        item.className = "version-item" + (v.latest ? " latest" : "");
        const file = v.file || "downloads/minimal_desktop.iso";
        const label = v.label || ("v" + (v.version || "unknown"));
        const version = v.version || "";
        const date = v.date || "";
        const sizeText = v.size ? formatBytes(Number(v.size)) : "unknown size";
        const notes = v.notes || "";
        const sha = v.sha256 || "";
        item.innerHTML = `
          <div class="version-main">
            <div class="version-head">
              <h3 class="version-title">${escapeHtml(label)}</h3>
              ${v.latest ? '<span class="version-badge">latest</span>' : ''}
              ${version ? `<span class="version-badge">${escapeHtml(version)}</span>` : ''}
            </div>
            <div class="version-meta">
              ${date ? `<span>Date: <code>${escapeHtml(date)}</code></span>` : ''}
              <span>Size: <code>${escapeHtml(sizeText)}</code></span>
              <span>File: <code>${escapeHtml(file.replace(/^downloads\//, ""))}</code></span>
            </div>
            ${notes ? `<p class="version-notes">${escapeHtml(notes)}</p>` : ''}
            ${sha ? `<p class="version-hash">SHA256: <code>${escapeHtml(sha)}</code></p>` : ''}
          </div>
          <div class="version-actions">
            <a class="btn btn-primary btn-download" href="${escapeAttr(file)}" download>
              <span>Download ISO</span><small>${escapeHtml(sizeText)}</small>
            </a>
            ${v.latest ? '<a class="btn" href="downloads/minimal_desktop.iso" download>Latest alias</a>' : ''}
          </div>`;
        container.appendChild(item);
      });
    } catch (e) {
      container.innerHTML = '<p class="versions-empty">Could not load versions. If you opened this page locally, ensure <code>downloads/versions.json</code> exists.</p>';
    }
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  }
  function escapeAttr(s) {
    return escapeHtml(s).replace(/"/g, "&quot;");
  }

  /* =========================================================
     GitHub + suggestions
     ========================================================= */
  /* Set REPO.user once the repository is published; every [data-gh] link
   * below follows it automatically (nav, footer, issue buttons). */
  const REPO = { user: "ardaerdem1890-ship-it", name: "minimal-desktop" };
  const repoBase = REPO.user ? `https://github.com/${REPO.user}/${REPO.name}` : "https://github.com";
  const repoReady = Boolean(REPO.user);

  const ghLinks = {
    repo: repoBase,
    issues: `${repoBase}/issues`,
    new: `${repoBase}/issues/new`,
    releases: `${repoBase}/releases`
  };
  document.querySelectorAll("[data-gh]").forEach((el) => {
    const kind = el.getAttribute("data-gh");
    el.setAttribute("href", ghLinks[kind] || repoBase);
    el.setAttribute("target", "_blank");
    el.setAttribute("rel", "noopener");
  });

  const SUG_KEY = "md.suggestions.v1";
  const AREA_LABEL = {
    bug: "Bug / crash", graphics: "Graphics & GUI", keyboard: "Keyboard & layouts",
    network: "Networking", install: "Installer & disk", feature: "Feature request"
  };

  const suggestForm = document.getElementById("suggest-form");
  const titleEl = document.getElementById("suggest-title");
  const areaEl = document.getElementById("suggest-area");
  const detailsEl = document.getElementById("suggest-details");
  const noteEl = document.getElementById("suggest-note");
  const issueEl = document.getElementById("suggest-issue");
  const savedEl = document.getElementById("suggest-saved");
  const listEl = document.getElementById("suggest-list");
  const countEl = document.getElementById("suggest-count");
  const clearEl = document.getElementById("suggest-clear");

  function loadSuggestions() {
    try {
      const raw = localStorage.getItem(SUG_KEY);
      const arr = raw ? JSON.parse(raw) : [];
      return Array.isArray(arr) ? arr : [];
    } catch (e) {
      return [];
    }
  }

  function storeSuggestions(arr) {
    try { localStorage.setItem(SUG_KEY, JSON.stringify(arr.slice(0, 50))); } catch (e) { /* private mode */ }
  }

  function issueUrl(title, area, details) {
    const body = [
      details || "(no details)",
      "",
      "---",
      `Area: ${AREA_LABEL[area] || area}`,
      `Page: ${location.href}`,
      `User agent: ${navigator.userAgent}`
    ].join("\n");
    const params = new URLSearchParams({
      title: title || "Suggestion",
      body,
      labels: area === "bug" ? "bug" : "enhancement"
    });
    return `${ghLinks.new}?${params.toString()}`;
  }

  function refreshIssueLink() {
    if (!issueEl) return;
    if (!repoReady) {
      issueEl.setAttribute("href", "#");
      issueEl.setAttribute("aria-disabled", "true");
      return;
    }
    issueEl.removeAttribute("aria-disabled");
    issueEl.setAttribute("href", issueUrl(titleEl.value.trim(), areaEl.value, detailsEl.value.trim()));
  }

  function renderSuggestions() {
    if (!listEl) return;
    const arr = loadSuggestions();
    listEl.innerHTML = "";
    if (countEl) countEl.textContent = arr.length ? `${arr.length} saved` : "";
    if (savedEl) savedEl.hidden = arr.length === 0;
    arr.forEach((item) => {
      const li = document.createElement("li");
      const tag = document.createElement("span");
      tag.className = "issue-tag " + (item.area === "bug" ? "bug" : item.area === "feature" ? "wish" : "fix");
      tag.textContent = AREA_LABEL[item.area] || item.area || "note";
      const body = document.createElement("div");
      const strong = document.createElement("strong");
      strong.textContent = item.title;
      body.appendChild(strong);
      if (item.details) {
        const p = document.createElement("span");
        p.className = "suggest-body";
        p.textContent = item.details;
        body.appendChild(p);
      }
      const time = document.createElement("time");
      time.textContent = new Date(item.at || Date.now()).toLocaleDateString();
      li.append(tag, body, time);
      listEl.appendChild(li);
    });
  }

  if (suggestForm) {
    [titleEl, areaEl, detailsEl].forEach((el) => {
      if (el) el.addEventListener("input", refreshIssueLink);
      if (el) el.addEventListener("change", refreshIssueLink);
    });

    suggestForm.addEventListener("submit", (ev) => {
      ev.preventDefault();
      const title = (titleEl.value || "").trim();
      if (!title) {
        noteEl.textContent = "Give the suggestion a short title first.";
        noteEl.classList.remove("ok");
        titleEl.focus();
        return;
      }
      const arr = loadSuggestions();
      arr.unshift({ title, area: areaEl.value, details: (detailsEl.value || "").trim(), at: Date.now() });
      storeSuggestions(arr);
      renderSuggestions();
      noteEl.textContent = repoReady
        ? "Saved in this browser. Use “Open as GitHub issue” to post it to the tracker."
        : "Saved in this browser. GitHub publishing is pending — nothing was sent anywhere.";
      noteEl.classList.add("ok");
      suggestForm.reset();
      refreshIssueLink();
    });
  }

  if (clearEl) {
    clearEl.addEventListener("click", () => {
      storeSuggestions([]);
      renderSuggestions();
      if (noteEl) { noteEl.textContent = "Cleared local suggestions."; noteEl.classList.add("ok"); }
    });
  }

  if (!repoReady && issueEl) {
    issueEl.title = "The GitHub repository is published with v1.4.0; the link goes live then.";
  }
  if (!repoReady && noteEl) {
    noteEl.textContent = "GitHub publishing is pending: suggestions stay in this browser for now.";
  }
  renderSuggestions();
  refreshIssueLink();

  loadVersions();

  /* clock in the footer year */
  const yearEl = document.getElementById("year");
  if (yearEl) yearEl.textContent = new Date().getFullYear();
})();
