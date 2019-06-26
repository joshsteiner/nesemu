if #arg ~= 1 then
  io.write"USAGE: runtests romfile.nes\n"
  os.exit(-1)
end

function exfmt(s, ...)
  local ss = string.format(s, ...)
  io.write(ss .. "\n")
  os.execute(ss)
end

wd = "~/Projects/nesemu"
romfile = arg[1]
nesemulog = "nesemulog"
fceuxlog = "fceuxlog"

io.write"running 2s...\n"
exfmt([[timeout 2s %s/nesemu "%s" 2> %s]], wd, romfile, nesemulog)
exfmt([[fceux "%s" --loadlua %s/scripts/fceuxlog.lua > %s]], romfile, wd, fceuxlog)
io.write"done"

io.write"fceux log:\n"
exfmt("head -20 %s", fceuxlog)

io.write"nesemu log:\n"
exfmt("head -20 %s", nesemulog)

