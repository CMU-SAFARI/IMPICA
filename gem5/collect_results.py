import sys
import re

if len(sys.argv) < 2:
    print("No input file name")
    exit(0)

term_file = open(sys.argv[1] + "system.terminal", "r")
for line in term_file:
    st_re = re.compile("PASS! SimTime = (\d+),")
    stats_re = re.compile("\[summary\] txn_cnt=(\d+), abort_cnt=\d+, run_time=0.(\d+), time_wait=0.(\d+), time_ts_alloc=0.\d+, time_man=0.(\d+), time_index=0.(\d+), time_abort=0.\d+, time_cleanup=0.(\d+)")
    m = st_re.match(line)
    if (m):
        sim_time = m.group(1)
    stat_m = stats_re.match(line)
term_file.close()


bw_stat = [0, 0]
power_stat = [0.0, 0.0, 0, 0]
cache_stat = [0.0, 0.0, 0.0, 0.0]
pim_stat = []
sim_inst = 0
for i in range(12):
    pim_stat.append(0)
print pim_stat

stats_file = open(sys.argv[1] + "stats.txt", "r")
for line in stats_file:
    hw_stat_re = re.compile("([a-zA-Z0-9_\.\:]+)\s+(\d+(\.\d*)?|\.\d+)\s+#")
    end_re = re.compile("[-]+\s+End Simulation Statistics")
    hw_stat_m = hw_stat_re.match(line)

    if (hw_stat_m):
        stat_name = hw_stat_m.group(1)
        if (stat_name == "sim_insts"):
            sim_inst = int(hw_stat_m.group(2))
        if (stat_name == "system.mem_ctrls.bw_total::realview.pimDevice"):
            bw_stat[0] = int(hw_stat_m.group(2))
        elif (stat_name == "system.mem_ctrls.bw_total::total"):
            bw_stat[1] = int(hw_stat_m.group(2))
        elif (stat_name == "system.mem_ctrls.averagePower::0"):
            power_stat[0] = float(hw_stat_m.group(2))
        elif (stat_name == "system.mem_ctrls.averagePower::1"):
            power_stat[1] = float(hw_stat_m.group(2))
        elif (stat_name == "system.mem_ctrls.totalEnergy::0"):
            power_stat[2] = int(hw_stat_m.group(2))
        elif (stat_name == "system.mem_ctrls.totalEnergy::1"):
            power_stat[3] = int(hw_stat_m.group(2))
        elif (stat_name == "system.l2.overall_miss_rate::total"):
            cache_stat[0] = float(hw_stat_m.group(2))
        elif (stat_name == "system.l2.overall_avg_miss_latency::total"):
            cache_stat[1] = float(hw_stat_m.group(2))
        elif (stat_name == "system.iocache.overall_miss_rate::total"):
            cache_stat[2] = float(hw_stat_m.group(2))
        elif (stat_name == "system.iocache.overall_avg_miss_latency::total"):
            cache_stat[3] = float(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalLatency::0"):
            pim_stat[0] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalLatency::1"):
            pim_stat[1] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalLatency::2"):
            pim_stat[2] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalLatency::3"):
            pim_stat[3] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalVaToPaLatency::0"):
            pim_stat[4] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalVaToPaLatency::1"):
            pim_stat[5] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalVaToPaLatency::2"):
            pim_stat[6] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalVaToPaLatency::3"):
            pim_stat[7] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalBtNodeLatency::0"):
            pim_stat[8] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalBtNodeLatency::1"):
            pim_stat[9] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalBtNodeLatency::2"):
            pim_stat[10] = int(hw_stat_m.group(2))
        elif (stat_name == "system.realview.pimDevice.totalBtNodeLatency::3"):
            pim_stat[11] = int(hw_stat_m.group(2))

    end_m = end_re.match(line)
    if end_m:
        print end_m.group()
        break
    

print "==overall=="
print stat_m.group(1)         #txn count
print sim_time                #sim time
print sim_inst

print "==time distribution=="
for i in range(2, 7):
    print stat_m.group(i)

print "==dram bandwidth=="
for i in range(2):
    print bw_stat[i]

print "==energy=="
for i in range(4):
    print power_stat[i]

print "==cache=="
for i in range(4):
    print cache_stat[i]

print "==pim=="
for i in range(12):
    print pim_stat[i]

