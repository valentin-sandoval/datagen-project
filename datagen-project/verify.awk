#!/usr/bin/awk -f
BEGIN { FS=","; max=0; okdup=1; okseq=1 }
NR==1 { next }
{
  c[$1]++
  if ($1 > max) max = $1
}
END {
  for (i in c) if (c[i] > 1) okdup = 0
  for (i = 1; i <= max; i++) if (!(i in c)) okseq = 0
  print "Sin duplicados:", okdup ? "SI" : "NO"
  print "Correlativos:",  okseq ? "SI" : "NO"
}
