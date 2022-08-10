set terminal png font "Calibri,14" size 640,320

capacity_tb=20
capacity_mb=capacity_tb*1000*1000
hdd_mbps=200
seconds_in_hour=3600

amplification=10 # need to read 8 pieces and then write one
# for legacy, ignore amplification since you are just bound to one drive

dcr_rebuild_rate(x)=hdd_mbps*(x-1) # subtract out the failed drive
dcr_rebuild_hours(x)=((capacity_mb*amplification)/dcr_rebuild_rate(x))/3600
legacy_rebuild_hours(x)=(capacity_mb/hdd_mbps)/3600

set yrange [0:30]
set xrange [10:170]

set key above
unset title
set ylabel 'Rebuild Time (hours)'
set xlabel 'Number of Drives'
set output 'declustered_rebuild.png'
plot dcr_rebuild_hours(x) lw 2 t "Declustered Erasure Groups", legacy_rebuild_hours(x) lw 4 t "Fixed Erasure Groups"
