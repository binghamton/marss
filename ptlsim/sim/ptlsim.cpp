//
// PTLsim: Cycle Accurate x86-64 Simulator
// Shared Functions and Structures
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <ptlsim.h>
#include <datastore.h>
#define CPT_STATS
#include <stats.h>
#undef CPT_STATS
#ifndef NEW_CACHE
#include <MemoryConfig.h>
#else
#include <memoryStats.h>
#endif
#include <elf.h>

#include <fstream>
#include <syscalls.h>

#include <ptl-qemu.h>

#ifndef CONFIG_ONLY
//
// Global variables
//
PTLsimConfig config;
ConfigurationParser<PTLsimConfig> configparser;
PTLsimStats stats;
PTLsimMachine ptl_machine;

ofstream ptl_logfile;
#ifdef TRACE_RIP
ofstream ptl_rip_trace;
#endif
ofstream trace_mem_logfile;
bool logenable = 0;
W64 sim_cycle = 0;
W64 unhalted_cycle_count = 0;
W64 iterations = 0;
W64 total_uops_executed = 0;
W64 total_uops_committed = 0;
W64 total_user_insns_committed = 0;
W64 total_basic_blocks_committed = 0;

W64 last_printed_status_at_ticks;
W64 last_printed_status_at_user_insn;
W64 last_printed_status_at_cycle;
W64 ticks_per_update;

W64 last_stats_captured_at_cycle = 0;
W64 tsc_at_start ;

#endif

void PTLsimConfig::reset() {
#ifdef PTLSIM_HYPERVISOR
  domain = (W64)(-1);
  run = 0;
  stop = 0;
  native = 0;
  kill = 0;
  flush_command_queue = 0;
  simswitch = 0;
#endif

  quiet = 0;
  core_name = "ooo";
  log_filename = "ptlsim.log";
  loglevel = 0;
  start_log_at_iteration = 0;
  start_log_at_rip = INVALIDRIP;
  log_on_console = 0;
  log_ptlsim_boot = 0;
  log_buffer_size = 524288;
  log_file_size = 1<<26;
  mm_logfile.reset();
  mm_log_buffer_size = 16384;
  enable_inline_mm_logging = 0;
  enable_mm_validate = 0;

  event_log_enabled = 0;
  event_log_ring_buffer_size = 32768;
  flush_event_log_every_cycle = 0;
  log_backwards_from_trigger_rip = INVALIDRIP;
  dump_state_now = 0;
  abort_at_end = 0;
  log_trigger_virt_addr_start = 0;
  log_trigger_virt_addr_end = 0;

  mem_event_log_enabled = 0;
  mem_event_log_ring_buffer_size = 262144;
  mem_flush_event_log_every_cycle = 0;

  atomic_bus_enabled = 0;
  verify_cache = 0;
  comparing_cache = 0;
  trace_memory_updates = 0;
  trace_memory_updates_logfile = "ptlsim.mem.log";
  stats_filename.reset();
  snapshot_cycles = infinity;
  snapshot_now.reset();

#ifndef PTLSIM_HYPERVISOR
  // Starting Point
  start_at_rip = INVALIDRIP;
  include_dyn_linker = 1;
  trigger_mode = 0;
  pause_at_startup = 0;
#endif

  //prefetcher
  use_L1_IP_based_prefetcher = 0;
  use_L2_IP_based_prefetcher = 0;
  use_L1_nextline_prefetcher = 0;
  use_L2_nextline_prefetcher = 0;
  use_GHB_prefetcher = 0;
  prefetch_own_line = 0;
  stride_prefetcher = 0;
  distance_prefetcher = 0;
  prefetch_degree = 1;
  wait_all_finished = 0;
  
  perfect_L2 = 0;

  // memory model
  use_memory_model = 0;
  use_new_memory_system = 1;
  kill_after_run = 1;
  stop_at_user_insns = infinity;
  stop_at_cycle = infinity;
  stop_at_iteration = infinity;
  stop_at_rip = INVALIDRIP;
  stop_at_marker = infinity;
  stop_at_marker_hits = infinity;
  stop_at_user_insns_relative = infinity;
  insns_in_last_basic_block = 65536;
  flush_interval = infinity;
#ifdef PTLSIM_HYPERVISOR
  event_trace_record_filename.reset();
  event_trace_record_stop = 0;
  event_trace_replay_filename.reset();

  core_freq_hz = 0;
  // default timer frequency is 100 hz in time-xen.c:
  timer_interrupt_freq_hz = 100;
  pseudo_real_time_clock = 0;
  realtime = 0;
  mask_interrupts = 0;
  console_mfn = 0;
  pause = 0;
  perfctr_name.reset();
  force_native = 0;
  kill_after_finish = 0;
  exit_after_finish = 0;
#endif

  continuous_validation = 0;
  validation_start_cycle = 0;

  perfect_cache = 0;

  dumpcode_filename = "test.dat";
  dump_at_end = 0;
  overshoot_and_dump = 0;
  bbcache_dump_filename.reset();

#ifndef PTLSIM_HYPERVISOR
  sequential_mode_insns = 0;
  exit_after_fullsim = 0;
#endif

  ///
  /// memory hierarchy implementation
  ///

  use_memory_hierarchy = 0;
  number_of_cores = 1;
  cores_per_L2 = 1;
  max_L1_req = 16;
  cache_config_type = "private_L2";
  use_shared_L3 = 0;
}

template <>
void ConfigurationParser<PTLsimConfig>::setup() {
#ifdef PTLSIM_HYPERVISOR
  // Full system only
  section("PTLmon Control");
  add(domain,                       "domain",               "Domain to access");

  section("Action (specify only one)");
  add(run,                          "run",                  "Run under simulation");
  add(stop,                         "stop",                 "Stop current simulation run and wait for command");
  add(native,                       "native",               "Switch to native mode");
  add(kill,                         "kill",                 "Kill PTLsim inside domain (and ptlmon), then shutdown domain");
  add(flush_command_queue,          "flush",                "Flush all queued commands, stop the current simulation run and wait");
  add(simswitch,                    "switch",               "Switch back to PTLsim while in native mode");
#endif

  section("Simulation Control");

  add(core_name,                    "core",                 "Run using specified core (-core <corename>)");

  section("General Logging Control");
  add(quiet,                        "quiet",                "Do not print PTLsim system information banner");
  add(log_filename,                 "ptl_logfile",              "Log filename (use /dev/fd/1 for stdout, /dev/fd/2 for stderr)");
  add(loglevel,                     "loglevel",             "Log level (0 to 99)");
  add(start_log_at_iteration,       "startlog",             "Start logging after iteration <startlog>");
  add(start_log_at_rip,             "startlogrip",          "Start logging after first translation of basic block starting at rip");
  add(log_on_console,               "consolelog",           "Replicate log file messages to console");
  add(log_ptlsim_boot,              "bootlog",              "Log PTLsim early boot and injection process (for debugging)");
  add(log_buffer_size,              "logbufsize",           "Size of PTLsim ptl_logfile buffer (not related to -ringbuf)");
  add(log_file_size,                "logfilesize",           "Size of PTLsim ptl_logfile");
  add(dump_state_now,               "dump-state-now",       "Dump the event log ring buffer and internal state of the active core");
  add(abort_at_end,                 "abort-at-end",         "Abort current simulation after next command (don't wait for next x86 boundary)");
  add(mm_logfile,                   "mm-ptl_logfile",           "Log PTLsim memory manager requests (alloc, free) to this file (use with ptlmmlog)");
  add(mm_log_buffer_size,           "mm-logbuf-size",       "Size of PTLsim memory manager log buffer (in events, not bytes)");
  add(enable_inline_mm_logging,     "mm-log-inline",        "Print every memory manager request in the main log file");
  add(enable_mm_validate,           "mm-validate",          "Validate every memory manager request against internal structures (slow)");

  section("Event Ring Buffer Logging Control");
  add(event_log_enabled,            "ringbuf",              "Log all core events to the ring buffer for backwards-in-time debugging");
  add(event_log_ring_buffer_size,   "ringbuf-size",         "Core event log ring buffer size: only save last <ringbuf> entries");
  add(flush_event_log_every_cycle,  "flush-events",         "Flush event log ring buffer to ptl_logfile after every cycle");
  add(log_backwards_from_trigger_rip,"ringbuf-trigger-rip", "Print event ring buffer when first uop in this rip is committed");
  add(log_trigger_virt_addr_start,   "ringbuf-trigger-virt-start", "Print event ring buffer when any virtual address in this range is touched");
  add(log_trigger_virt_addr_end,     "ringbuf-trigger-virt-end",   "Print event ring buffer when any virtual address in this range is touched");

  section("Statistics Database");
  add(stats_filename,               "stats",                "Statistics data store hierarchy root");
  add(snapshot_cycles,              "snapshot-cycles",      "Take statistical snapshot and reset every <snapshot> cycles");
  add(snapshot_now,                 "snapshot-now",         "Take statistical snapshot immediately, using specified name");
#ifndef PTLSIM_HYPERVISOR
  // Userspace only
  section("Start Point");
  add(start_at_rip,                 "startrip",             "Start at rip <startrip>");
  add(include_dyn_linker,           "excludeld",            "Exclude dynamic linker execution");
  add(trigger_mode,                 "trigger",              "Trigger mode: wait for user process to do simcall before entering PTL mode");
  add(pause_at_startup,             "pause-at-startup",     "Pause for N seconds after starting up (to allow debugger to attach)");
#endif

  section("Trace Stop Point");
  add(stop_at_user_insns,           "stopinsns",            "Stop after executing <stopinsns> user instructions");
  add(stop_at_cycle,                "stopcycle",            "Stop after <stop> cycles");
  add(stop_at_iteration,            "stopiter",             "Stop after <stop> iterations (does not apply to cycle-accurate cores)");  
  add(stop_at_rip,                  "stoprip",              "Stop before rip <stoprip> is translated for the first time");
  add(stop_at_marker,               "stop-at-marker",       "Stop after PTLCALL_MARKER with marker X");
  add(stop_at_marker_hits,          "stop-at-marker-hits",  "Stop after PTLCALL_MARKER is called N times");
  add(stop_at_user_insns_relative,  "stopinsns-rel",        "Stop after executing <stopinsns-rel> user instructions relative to start of current run");
  add(insns_in_last_basic_block,    "bbinsns",              "In final basic block, only translate <bbinsns> user instructions");
  add(flush_interval,               "flushevery",           "Flush the pipeline every N committed instructions");
  add(kill_after_run,               "kill-after-run",       "Kill PTLsim after this run");

#ifdef PTLSIM_HYPERVISOR
  // Full system only
  section("Event Trace Recording");
  add(event_trace_record_filename,  "event-record",         "Save replayable events (interrupts, DMAs, etc) to this file");
  add(event_trace_record_stop,      "event-record-stop",    "Stop recording events");
  add(event_trace_replay_filename,  "event-replay",         "Replay events (interrupts, DMAs, etc) to this file, starting at checkpoint");

  section("Timers and Interrupts");
  add(core_freq_hz,                 "corefreq",             "Core clock frequency in Hz (default uses host system frequency)");
  add(timer_interrupt_freq_hz,      "timerfreq",            "Timer interrupt frequency in Hz");
  add(pseudo_real_time_clock,       "pseudo-rtc",           "Real time clock always starts at time saved in checkpoint");
  add(realtime,                     "realtime",             "Operate in real time: no time dilation (not accurate for I/O intensive workloads!)");
  add(mask_interrupts,              "maskints",             "Mask all interrupts (required for guaranteed deterministic behavior)");
  add(console_mfn,                  "console-mfn",          "Track the specified Xen console MFN");
  add(pause,                        "pause",                "Pause domain after using -native");
  add(perfctr_name,                 "perfctr",              "Performance counter generic name for hardware profiling during native mode");
  add(force_native,                 "force-native",         "Force native mode: ignore attempts to switch to simulation");
  add(kill_after_finish,            "kill-after-finish",     "kill both simulator and domainU after finish simulation");
  add(exit_after_finish,            "exit-after-finish",     "exit simulator keep domainU after finish simulation");
#endif

  section("Validation");
  add(continuous_validation,        "validate",             "Continuous validation: validate against known-good sequential model");
  add(validation_start_cycle,       "validate-start-cycle", "Start continuous validation after N cycles");

  section("Out of Order Core (ooocore)");
  add(perfect_cache,                "perfect-cache",        "Perfect cache performance: all loads and stores hit in L1");

  section("Miscellaneous");
  add(dumpcode_filename,            "dumpcode",             "Save page of user code at final rip to file <dumpcode>");
  add(dump_at_end,                  "dump-at-end",          "Set breakpoint and dump core before first instruction executed on return to native mode");
  add(overshoot_and_dump,           "overshoot-and-dump",   "Set breakpoint and dump core after first instruction executed on return to native mode");
  add(bbcache_dump_filename,        "bbdump",               "Basic block cache dump filename");
#ifndef PTLSIM_HYPERVISOR
  // Userspace only
  add(sequential_mode_insns,        "seq",                  "Run in sequential mode for <seq> instructions before switching to out of order");
  add(exit_after_fullsim,           "exitend",              "Kill the thread after full simulation completes rather than going native");
#endif
  // for prefetcher
  add(use_L1_IP_based_prefetcher,           "L1-IP-based-prefetch",              "use L1 IP based stride prefetcher, which will fetch to L1");
  add(use_L2_IP_based_prefetcher,           "L2-IP-based-prefetch",              "use L2 IP based stride prefetcher, which will fetch to L2 not L1");

  add(use_L1_nextline_prefetcher,           "L1-nextline-prefetch",              "use L1 next line prefetcher, which will fetch to L1");
  add(use_L2_nextline_prefetcher,           "L2-nextline-prefetch",              "use L2 next line prefetcher, which will only fetch to L2 not L1");

  // for GHB prefetcher
  add(use_GHB_prefetcher,           "use_GHB_prefetcher",              "use GHB prefetcher, which will only fetch to L2 not L1");

 add(stride_prefetcher,           "stride-prefetcher",              "use GHB stride prefetcher");
 add(distance_prefetcher,         "distance-prefetcher",              "use GHB distance correlation prefetcher");
 add(prefetch_degree,             "prefetch-degree",              "the number of prefetch issued per miss");
 add(use_memory_model,             "use-memory-model",              "model the memory latency, memory bank contention, bus contention");
 add(wait_all_finished,             "wait-all-finished",              "wait all threads reach total number of insn before exit");
 add(perfect_L2,                   "perfect-L2",             " L2 always hit");

 add(prefetch_own_line,           "prefetch-own-line",              "the prefetch data only write to the cache line of same thread");
 add(use_new_memory_system,      "use-new-memory-system",          "use more detailed memory simulation(set by default)");
 add(verify_cache,               "verify-cache",                   "run simulation with storing actual data in cache");
 add(comparing_cache,               "comparing-cache",                   "run simulation with storing actual data in cache");
 add(trace_memory_updates,               "trace-memory-updates",                   "log memory updates");
 add(trace_memory_updates_logfile,        "trace-memory-updates-ptl_logfile",                   "ptl_logfile for memory updates");
 
 section("Memory Event Ring Buffer Logging Control");
 add(mem_event_log_enabled,            "mem-ringbuf",              "Log all core events to the ring buffer for backwards-in-time debugging");
 add(mem_event_log_ring_buffer_size,   "mem-ringbuf-size",         "Core event log ring buffer size: only save last <ringbuf> entries");
 add(mem_flush_event_log_every_cycle,  "mem-flush-events",         "Flush event log ring buffer to ptl_logfile after every cycle");

 section("bus configuration");
  add(atomic_bus_enabled,               "atomic_bus",               "Using single atomic bus instead of split bus");

 ///
 /// following are for the new memory hierarchy implementation:
 ///

  section("Memory Hierarchy Configuration");
  //  add(memory_log,               "memory-log",               "log memory debugging info");
  add(use_memory_hierarchy,               "use-memory-hierarchy",               "Using new memory hierarchy implementation");
  add(number_of_cores,               "number-of-cores",               "number of cores");
  add(cores_per_L2,               "cores-per-L2",               "number of cores sharing a L2 cache");
  add(max_L1_req,               "max-L1-req",               "max number of L1 requests");
  add(cache_config_type,               "cache-config-type",               "possible config are shared_L2, private_L2");
  add(use_shared_L3,               "use-shared-L3",               "set true to used a shared L3");
};

#ifndef CONFIG_ONLY

ostream& operator <<(ostream& os, const PTLsimConfig& config) {
  return configparser.print(os, config);
}

void print_banner(ostream& os, const PTLsimStats& stats, int argc, char** argv) {
  utsname hostinfo;
  sys_uname(&hostinfo);

  os << "//  ", endl;
#ifdef __x86_64__
#ifdef PTLSIM_HYPERVISOR
  os << "//  PTLsim: Cycle Accurate x86-64 Full System SMP/SMT Simulator", endl;
#else
  os << "//  PTLsim: Cycle Accurate x86-64 Simulator", endl;
#endif
#else
  os << "//  PTLsim: Cycle Accurate x86 Simulator (32-bit version)", endl;
#endif
  os << "//  Copyright 1999-2007 Matt T. Yourst <yourst@yourst.com>", endl;
  os << "// ", endl;
  os << "//  Revision ", stringify(SVNREV), " (", stringify(SVNDATE), ")", endl;
  os << "//  Built ", __DATE__, " ", __TIME__, " on ", stringify(BUILDHOST), " using gcc-", 
    stringify(__GNUC__), ".", stringify(__GNUC_MINOR__), endl;
  os << "//  Running on ", hostinfo.nodename, ".", hostinfo.domainname, endl;
  os << "//  ", endl;
#ifndef PTLSIM_HYPERVISOR
  os << "//  Arguments: ";
  foreach (i, argc) {
    os << argv[i];
    if (i != (argc-1)) os << ' ';
  }
  os << endl;
  os << "//  Thread ", sys_getpid(), " is running in ", (ctx.use64 ? "64-bit x86-64" : "32-bit x86"), " mode", endl;
  os << "//  ", endl;
#endif
  os << endl;
  os << flush;
}

void collect_common_sysinfo(PTLsimStats& stats) {
  utsname hostinfo;
  sys_uname(&hostinfo);

  stringbuf sb;
#define strput(x, y) (strncpy((x), (y), sizeof(x)))

  sb.reset(); sb << __DATE__, " ", __TIME__;
  strput(stats.simulator.version.build_timestamp, sb);
//  stats.simulator.version.svn_revision = SVNREV;
  strput(stats.simulator.version.svn_timestamp, stringify(SVNDATE));
  strput(stats.simulator.version.build_hostname, stringify(BUILDHOST));
  sb.reset(); sb << "gcc-", __GNUC__, ".", __GNUC_MINOR__;
  strput(stats.simulator.version.build_compiler, sb);

  stats.simulator.run.timestamp = sys_time(0);
  sb.reset(); sb << hostinfo.nodename, ".", hostinfo.domainname;
  strput(stats.simulator.run.hostname, sb);
  stats.simulator.run.native_hz = get_core_freq_hz();
  strput(stats.simulator.run.kernel_version, hostinfo.release);
}

void print_usage(int argc, char** argv) {
  cerr << "Syntax: ptlsim <executable> <arguments...>", endl;
  cerr << "All other options come from file /home/<username>/.ptlsim/path/to/executable", endl, endl;

  configparser.printusage(cerr, config);
}

stringbuf current_stats_filename;
stringbuf current_log_filename;
stringbuf current_bbcache_dump_filename;
stringbuf current_trace_memory_updates_logfile;

void backup_and_reopen_logfile() {
  if (config.log_filename) {
    if (ptl_logfile) ptl_logfile.close();
    stringbuf oldname;
    oldname << config.log_filename, ".backup";
    sys_unlink(oldname);
    sys_rename(config.log_filename, oldname);
    ptl_logfile.open(config.log_filename);
//#ifdef TRACE_RIP
//	ptl_rip_trace.open("ptl_rip_trace");
//#endif
  }
}

void backup_and_reopen_memory_logfile() {
  if (config.trace_memory_updates_logfile) {
    if (trace_mem_logfile) trace_mem_logfile.close();
    stringbuf oldname;
    oldname << config.trace_memory_updates_logfile, ".backup";
    sys_unlink(oldname);
    sys_rename(config.trace_memory_updates_logfile, oldname);
    trace_mem_logfile.open(config.trace_memory_updates_logfile);
  }
}

void force_logging_enabled() {
  logenable = 1;
  config.start_log_at_iteration = 0;
  config.loglevel = 99;
  config.flush_event_log_every_cycle = 1;
}

extern byte _binary_ptlsim_build_ptlsim_dst_start;
extern byte _binary_ptlsim_build_ptlsim_dst_end;
StatsFileWriter statswriter;

void capture_stats_snapshot(const char* name) {
  if unlikely (!statswriter) return;

  if (logable(100)|1) {
    ptl_logfile << "Making stats snapshot uuid ", statswriter.next_uuid();
    if (name) ptl_logfile << " named ", name;
    ptl_logfile << " at cycle ", sim_cycle, endl;
  }

  if (PTLsimMachine::getcurrent()) {
    PTLsimMachine::getcurrent()->update_stats(stats);
  }

  setzero(stats.snapshot_name);

  if (name) {
    stringbuf sb;
    strncpy(stats.snapshot_name, name, sizeof(stats.snapshot_name));
  }

  stats.snapshot_uuid = statswriter.next_uuid();
  statswriter.write(&stats, name);
}

void flush_stats() {
  statswriter.flush();
}

void print_sysinfo(ostream& os) {
	// TODO: In QEMU based system 
}

bool handle_config_change(PTLsimConfig& config, int argc, char** argv) {
  static bool first_time = true;

  if (config.log_filename.set() && (config.log_filename != current_log_filename)) {
    // Can also use "-ptl_logfile /dev/fd/1" to send to stdout (or /dev/fd/2 for stderr):
    backup_and_reopen_logfile();
    current_log_filename = config.log_filename;
  }
#ifdef TRACE_RIP
	ptl_rip_trace.open("ptl_rip_trace");
#endif

//  ptl_logfile.setchain((config.log_on_console) ? &cout : null);

//  if (config.stats_filename.set() && (config.stats_filename != current_stats_filename)) {
    // Can also use "-ptl_logfile /dev/fd/1" to send to stdout (or /dev/fd/2 for stderr):
    statswriter.open(config.stats_filename, &_binary_ptlsim_build_ptlsim_dst_start,
                     &_binary_ptlsim_build_ptlsim_dst_end - &_binary_ptlsim_build_ptlsim_dst_start,
                     sizeof(PTLsimStats));
    current_stats_filename = config.stats_filename;
//  }

  if (config.trace_memory_updates_logfile.set() && (config.trace_memory_updates_logfile != current_trace_memory_updates_logfile)) {
    backup_and_reopen_memory_logfile();
    current_trace_memory_updates_logfile = config.trace_memory_updates_logfile;
  }


//  ptl_logfile.setbuf(config.log_buffer_size);

  if ((config.loglevel > 0) & (config.start_log_at_rip == INVALIDRIP) & (config.start_log_at_iteration == infinity)) {
    config.start_log_at_iteration = 0;
  }

  // Force printing every cycle if loglevel >= 100:
  if (config.loglevel >= 100) {
    config.event_log_enabled = 1;
    config.flush_event_log_every_cycle = 1;
  }

  //
  // Fix up parameter defaults:
  //
  if (config.start_log_at_rip != INVALIDRIP) {
    config.start_log_at_iteration = infinity;
    logenable = 0;
  } else if (config.start_log_at_iteration != infinity) {
    config.start_log_at_rip = INVALIDRIP;
    logenable = 0;
  }

  //  logenable = 1; // Hui

  if (config.bbcache_dump_filename.set() && (config.bbcache_dump_filename != current_bbcache_dump_filename)) {
    // Can also use "-ptl_logfile /dev/fd/1" to send to stdout (or /dev/fd/2 for stderr):
    bbcache_dump_file.open(config.bbcache_dump_filename);
    current_bbcache_dump_filename = config.bbcache_dump_filename;
  }

  if (config.log_trigger_virt_addr_start && (!config.log_trigger_virt_addr_end)) {
    config.log_trigger_virt_addr_end = config.log_trigger_virt_addr_start;
  }

  ptl_mm_set_logging(config.mm_logfile.set() ? (char*)(config.mm_logfile) : null, config.mm_log_buffer_size, config.enable_inline_mm_logging);
  ptl_mm_set_validate(config.enable_mm_validate);

#ifdef __x86_64__
  config.start_log_at_rip = signext64(config.start_log_at_rip, 48);
  config.log_backwards_from_trigger_rip = signext64(config.log_backwards_from_trigger_rip, 48);
#ifndef PTLSIM_HYPERVISOR
  config.start_at_rip = signext64(config.start_at_rip, 48);
#endif
  config.stop_at_rip = signext64(config.stop_at_rip, 48);
#endif

  if(config.run && !config.kill) {
	  start_simulation = 1;
  }

  if(config.kill) {
	  config.run = false;
  }

  if (first_time) {
    if (!config.quiet) {
#ifndef PTLSIM_HYPERVISOR
      print_banner(cerr, stats, argc, argv);
#endif
      print_sysinfo(cerr);
#ifdef PTLSIM_HYPERVISOR
      if (!(config.run | config.native | config.kill))
        cerr << "PTLsim is now waiting for a command.", endl, flush;
#endif
    }
    print_banner(ptl_logfile, stats, argc, argv);
    print_sysinfo(ptl_logfile);
    cerr << flush;
    ptl_logfile << config;
    ptl_logfile.flush();
    first_time = false;
  }

#ifdef PTLSIM_HYPERVISOR
  int total = config.run + config.stop + config.native + config.kill;
  if (total > 1) {
    ptl_logfile << "Warning: only one action (from -run, -stop, -native, -kill) can be specified at once", endl, flush;
    cerr << "Warning: only one action (from -run, -stop, -native, -kill) can be specified at once", endl, flush;
  }
#endif

  return true;
}

Hashtable<const char*, PTLsimMachine*, 1>* machinetable = null;

// Make sure the vtable gets compiled:
PTLsimMachine dummymachine;

bool PTLsimMachine::init(PTLsimConfig& config) { return false; }
int PTLsimMachine::run(PTLsimConfig& config) { return 0; }
void PTLsimMachine::update_stats(PTLsimStats& stats) { return; }
void PTLsimMachine::dump_state(ostream& os) { return; }
void PTLsimMachine::flush_tlb(Context& ctx) { return; }
void PTLsimMachine::flush_tlb_virt(Context& ctx, Waddr virtaddr) { return; }

void PTLsimMachine::addmachine(const char* name, PTLsimMachine* machine) {
  if unlikely (!machinetable) {
    machinetable = new Hashtable<const char*, PTLsimMachine*, 1>();
  }
//  cerr << "Adding machine ", name, endl;
  machinetable->add(name, machine);
  machine->first_run = 0;
}
void PTLsimMachine::removemachine(const char* name, PTLsimMachine* machine) {
  machinetable->remove(name, machine);
}

PTLsimMachine* PTLsimMachine::getmachine(const char* name) {
  if unlikely (!machinetable) return null;
  PTLsimMachine** p = machinetable->get(name);
  if (!p) return null;
  return *p;
}

// Currently executing machine model:
PTLsimMachine* curr_ptl_machine = null;

PTLsimMachine* PTLsimMachine::getcurrent() {
  return curr_ptl_machine;
}

void ptl_reconfigure(char* config_str) {
	char* argv[1]; argv[0] = config_str;
	configparser.parse(config, config_str);
	handle_config_change(config, 1, argv);
	ptl_logfile << "Configuration changed: ", config, endl;

	// set the curr_ptl_machine to null so it will be automatically changed to
	// new configured machine
	curr_ptl_machine = null;
}

extern "C" void ptl_machine_init(char* config_str) {
	configparser.setup();
	config.reset();

//OutOfOrderMachine ooomodel("ooo");
	// Setup the configuration
	ptl_reconfigure(config_str);

	// After reconfigure reset the machine's initalized variable
	
	PTLsimMachine* machine = null;
	char* machinename = config.core_name;
	if likely (curr_ptl_machine != null) {
		machine = curr_ptl_machine;
	} else {
		machine = PTLsimMachine::getmachine(machinename);
	}

	if(machine)
		machine->initialized = 0;

}

//Context* ptl_contexts[MAX_CONTEXTS];
//
//inline Context& contextof(W8 i) {
//	return *ptl_contexts[i];
//}

static int ctx_counter = 0;
extern "C"
CPUX86State* ptl_create_new_context() {

	assert(ctx_counter < contextcount);

	// Create a new CPU context and add it to contexts array
	Context* ctx = new Context();
	ptl_contexts[ctx_counter] = ctx;
	cerr << "Created new context(", ctx_counter, "):",
		contextof(ctx_counter), endl;
	ctx_counter++;

	return (CPUX86State*)(ctx);
}

// print selected stats to log for average of all cores
void print_stats_in_log(){
  

#ifdef PTLSIM_HYPERVISOR
  // 1. execution key stats:
  // uops_in_mode: kernel and user
  ptl_logfile << " kernel-insns ", (stats.external.total.insns_in_mode.kernel64 * 100.0) / total_user_insns_committed, endl;
  ptl_logfile << " user-insns ", (stats.external.total.insns_in_mode.user64 * 100.0) / total_user_insns_committed, endl;
  // cycles_in_mode: kernel and user
  ptl_logfile << " kernel-cycles ", (stats.external.total.cycles_in_mode.kernel64 * 100.0) / sim_cycle, endl;
  ptl_logfile << " user-cycles ", (stats.external.total.cycles_in_mode.user64 * 100.0) / sim_cycle, endl;
#endif
  //#define OPCLASS_BRANCH                  (OPCLASS_COND_BRANCH|OPCLASS_INDIR_BRANCH|OPCLASS_UNCOND_BRANCH|OPCLASS_ASSIST)

  //#define OPCLASS_LOAD                    (1 << 11)
  //#define OPCLASS_STORE                   (1 << 12)

  // opclass: load, store, branch,
  W64 total_uops = stats.ooocore_context_total.commit.uops;
  ptl_logfile << " total_uop ", total_uops, endl;

  ptl_logfile << " total_load ", stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_LOAD)], endl;
  ptl_logfile << " load_percentage ", (stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_LOAD)] * 100.0 )/ (total_uops * 1.0), endl;
  ptl_logfile << " total_store ", stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_STORE)], endl;
  ptl_logfile << " store_percentage ", (stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_STORE)] * 100.0)/(total_uops * 1.0), endl;
  ptl_logfile << " total_branch ", stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_BRANCH)], endl;
  ptl_logfile << " branch_percentage ", (stats.ooocore_context_total.commit.opclass[lsbindex(OPCLASS_BRANCH)] * 100.0)/(total_uops * 1.0), endl;
  // branch prediction accuracy
  ptl_logfile << " branch-accuracy ", double (stats.ooocore_context_total.branchpred.cond[1] * 100.0)/double (stats.ooocore_context_total.branchpred.cond[0] + stats.ooocore_context_total.branchpred.cond[1]), endl;

  // 2. simulation speed: 
  ptl_logfile << " elapse_seconds ", stats.elapse_seconds, endl;
  // CPS : number of similated cycle per second
  ptl_logfile << " CPS ",  W64(double(sim_cycle) / double(stats.elapse_seconds)), endl;
  // IPS : number of instruction commited per second
  ptl_logfile << " IPS ", W64(double(total_user_insns_committed) / double(stats.elapse_seconds)), endl;

  // 3. performance:
  // IPC : number of instruction per second
  ptl_logfile << " total_cycle ", sim_cycle, endl;
  ptl_logfile << " per_vcpu_IPC ", stats.ooocore_context_total.commit.ipc, endl;
  ptl_logfile << " total_IPC ",  double(total_user_insns_committed) / double(sim_cycle), endl;

  // 4. internconnection related 
  struct CacheStats::cpurequest::count &count_L1I = stats.memory.total.L1I.cpurequest.count;
  W64 hit_L1I = count_L1I.hit.read.hit.hit + count_L1I.hit.read.hit.forward +  count_L1I.hit.write.hit.hit + count_L1I.hit.write.hit.forward;
  W64 miss_L1I = count_L1I.miss.read +  count_L1I.miss.write;
  ptl_logfile << " L1I_hit_rate ", double(hit_L1I * 100.0) / double (hit_L1I + miss_L1I), endl;

  struct CacheStats::cpurequest::count &count_L1D = stats.memory.total.L1D.cpurequest.count;
  W64 hit_L1D = count_L1D.hit.read.hit.hit + count_L1D.hit.read.hit.forward +  count_L1D.hit.write.hit.hit + count_L1D.hit.write.hit.forward;
  W64 miss_L1D = count_L1D.miss.read +  count_L1D.miss.write;
  ptl_logfile << " L1D_hit_rate ", double(hit_L1D * 100.0) / double (hit_L1D + miss_L1D), endl;

  struct CacheStats::cpurequest::count &count_L2 = stats.memory.total.L2.cpurequest.count;
  W64 hit_L2 = count_L2.hit.read.hit.hit + count_L2.hit.read.hit.forward +  count_L2.hit.write.hit.hit + count_L2.hit.write.hit.forward;
  W64 miss_L2 = count_L2.miss.read +  count_L2.miss.write;
  ptl_logfile << " L2_hit_rate ", double(hit_L2 * 100.0) / double (hit_L2 + miss_L2), endl;

  // average load latency
  ptl_logfile << " L1I_IF_latency ", double (stats.memory.total.L1I.latency.IF) / double (stats.memory.total.L1I.lat_count.IF), endl;
  ptl_logfile << " L1D_load_latency ", double (stats.memory.total.L1D.latency.load) / double (stats.memory.total.L1D.lat_count.load), endl;
  ptl_logfile << " L1_read_latency ", double (stats.memory.total.L1I.latency.IF + stats.memory.total.L1D.latency.load) / double (stats.memory.total.L1I.lat_count.IF + stats.memory.total.L1D.lat_count.load), endl;
  ptl_logfile << " L1D_store_latency ", double (stats.memory.total.L1D.latency.store) / double (stats.memory.total.L1D.lat_count.store), endl;

  ptl_logfile << " L2_IF_latency ", double (stats.memory.total.L2.latency.IF) / double (stats.memory.total.L2.lat_count.IF), endl;
  ptl_logfile << " L2_load_latency ", double (stats.memory.total.L2.latency.load) / double (stats.memory.total.L2.lat_count.load), endl;
  ptl_logfile << " L2_read_latency ", double (stats.memory.total.L2.latency.IF + stats.memory.total.L2.latency.load) / double (stats.memory.total.L2.lat_count.IF + stats.memory.total.L2.lat_count.load), endl;
  ptl_logfile << " L2_store_latency ", double (stats.memory.total.L2.latency.store) / double (stats.memory.total.L2.lat_count.store), endl;

  // average load miss latency
   ptl_logfile << " L1_read_miss_latency ", double (stats.memory.total.L1I.latency.IF + stats.memory.total.L1D.latency.load) / double (count_L1I.miss.read +  count_L2.miss.read), endl;
   ptl_logfile << " L1_write_miss_latency ", double (stats.memory.total.L1D.latency.store) / double (count_L1D.miss.write), endl;

  
#ifndef NEW_CACHE
  // bus utilization
   ptl_logfile << " atomic_bus_utilization ", double (stats.memory.bus.atomic_utilization[Memory::BUS_DES_BUSY] * 100.0) / double (  stats.memory.bus.atomic_utilization[Memory::BUS_DES_FREE] +  stats.memory.bus.atomic_utilization[Memory::BUS_DES_BUSY] + 1), endl; 
   ptl_logfile << " addr_bus_utilization ", double (stats.memory.bus.addr_utilization[Memory::BUS_DES_BUSY] * 100.0) / double (  stats.memory.bus.addr_utilization[Memory::BUS_DES_FREE] +  stats.memory.bus.addr_utilization[Memory::BUS_DES_BUSY] +1), endl; 
   ptl_logfile << " data_bus_utilization ", double (stats.memory.bus.data_utilization[Memory::BUS_DES_BUSY] * 100.0) / double (  stats.memory.bus.data_utilization[Memory::BUS_DES_FREE] +  stats.memory.bus.data_utilization[Memory::BUS_DES_BUSY] +1), endl; 

   W64 total_bus_command = 0;
   W64 total_command_grant_latency = 0;
   W64 total_command_bus_latency = 0;

  // bus activity classification
//    foreach (i , Memory::NUM_BUS_COMMANDS){
//      if(!stats.memory.bus.command_count[i]) stats.memory.bus.command_count[i]=1; 
//      ptl_logfile << Memory::BusCommandName[i], "_count ", stats.memory.bus.command_count[i], endl;
//      int grant_latency =  double (stats.memory.bus.command_grant_latency[i]) / double (stats.memory.bus.command_count[i]);
//      // delay wait for bus.
//      ptl_logfile << Memory::BusCommandName[i], "_grant_latency ", grant_latency, endl;
//      int bus_latency =  double (stats.memory.bus.command_bus_latency[i]) / double (stats.memory.bus.command_count[i]);
//      // delay after bus granted.
//      ptl_logfile << Memory::BusCommandName[i], "_bus_latency ", bus_latency, endl;

//      if(i == Memory::BUS_COMMAND_RD){
//        ptl_logfile << " L2_read_miss_latency ", grant_latency+bus_latency, endl;
//      }

//      if(i == Memory::BUS_COMMAND_RDX){
//        ptl_logfile << " L2_write_miss_latency ", grant_latency+bus_latency, endl;
//      }

//      if(i == Memory::BUS_COMMAND_WB){
//        ptl_logfile << " L2_write_back_latency ", grant_latency+bus_latency, endl;
//      }

//      if(i == Memory::BUS_COMMAND_UPGR){
//        ptl_logfile << " L2_write_upgrade_latency ", grant_latency+bus_latency, endl;
//      }

//      total_bus_command += stats.memory.bus.command_count[i];
//      total_command_bus_latency += stats.memory.bus.command_bus_latency[i];
//      total_command_grant_latency += stats.memory.bus.command_grant_latency[i];
//    }
   
   foreach (i , Memory::NUM_BUS_COMMANDS){
     if(!stats.memory.bus.command_count[i]) stats.memory.bus.command_count[i]=1; 
     ptl_logfile << "total_",Memory::BusCommandName[i], "_count ", stats.memory.bus.command_count[i], endl;
     int grant_latency =  double (stats.memory.bus.command_grant_latency[i]) / double (stats.memory.bus.command_count[i]);
     // delay wait for bus.
     ptl_logfile << "average_",Memory::BusCommandName[i], "_grant_latency ", grant_latency, endl;
     int bus_latency =  double (stats.memory.bus.command_bus_latency[i]) / double (stats.memory.bus.command_count[i]);
     // delay after bus granted.
     ptl_logfile << "average_",Memory::BusCommandName[i], "_bus_latency ", bus_latency, endl;


     total_bus_command += stats.memory.bus.command_count[i];
     total_command_bus_latency += stats.memory.bus.command_bus_latency[i];
     total_command_grant_latency += stats.memory.bus.command_grant_latency[i];
   }

   foreach (j , Memory::NUM_BUS_COMMANDS){
     W64 total_miss_lat = stats.memory.bus.command_grant_latency[j] + stats.memory.bus.command_bus_latency[j];
     ptl_logfile << " L2_miss", Memory::BusCommandName[j], "_latency ", total_miss_lat, endl;
     ptl_logfile << " L2_miss", Memory::BusCommandName[j], "_latency_percentage ", (total_miss_lat *100.0) / ((total_command_grant_latency + total_command_bus_latency) * 1.0), endl;
   }

  // bus command latency average 
   ptl_logfile << "average_bus_grant_latency ", double(total_command_grant_latency) / double(total_bus_command), endl;
   ptl_logfile << "average_bus_latency ", double(total_command_bus_latency) / double(total_bus_command), endl;
   ptl_logfile << "average_bus_total_latency ", double(total_command_grant_latency + total_command_bus_latency) / double(total_bus_command), endl;
#endif // NEW_CACHE
}


extern "C" uint8_t ptl_simulate() {
	PTLsimMachine* machine = null;
	char* machinename = config.core_name;
	if likely (curr_ptl_machine != null) {
		machine = curr_ptl_machine;
	} else {
		machine = PTLsimMachine::getmachine(machinename);
	}

	if (!machine) {
		ptl_logfile << "Cannot find core named '", machinename, "'", endl;
		cerr << "Cannot find core named '", machinename, "'", endl;
		return 0;
	}

	if (!machine->initialized) {
		ptl_logfile << "Initializing core '", machinename, "'", endl;
		if (!machine->init(config)) {
			ptl_logfile << "Cannot initialize core model; check its configuration!", endl;
			return 0;
		}
		machine->initialized = 1;
		machine->first_run = 1;

		if(logable(1)) {
			ptl_logfile << "Switching to simulation core '", machinename, "'...", endl, flush;
			cerr <<  "Switching to simulation core '", machinename, "'...", endl, flush;
			ptl_logfile << "Stopping after ", config.stop_at_user_insns, " commits", endl, flush;
			cerr << "Stopping after ", config.stop_at_user_insns, " commits", endl, flush;
		}

		// Update stats every half second:
		ticks_per_update = seconds_to_ticks(0.2);
		//ticks_per_update = seconds_to_ticks(0.1);
		last_printed_status_at_ticks = 0;
		last_printed_status_at_user_insn = 0;
		last_printed_status_at_cycle = 0;

		tsc_at_start = rdtsc();
		curr_ptl_machine = machine;
	}

	foreach(ctx_no, contextcount) {
//		ptl_logfile << "Context[", ctx_no, "]:", endl, 
//					contextof(ctx_no), endl;
		Context& ctx = contextof(ctx_no);
		ctx.setup_ptlsim_switch();
//		ctx.cs_segment_updated();
//		ctx.set_eip_ptlsim();
//		ctx.update_mode(
//				(((CPUX86State*)(&ctx))->hflags & HF_CPL_MASK) == 0);
		ctx.running = 1;
	}

	if(machine->stopped != 0)
		machine->stopped = 0;

	if(logable(1))
		ptl_logfile << "Starting simulation at rip: ", (void*)contextof(0).get_cs_eip(), " kernel_mode: ", contextof(0).kernel_mode, endl;

	machine->run(config);

	if (config.stop_at_user_insns <= total_user_insns_committed || config.kill == true) {
		machine->stopped = 1;
	}

	if (!machine->stopped) {
		ptl_logfile << "Switching back to qemu rip: ", (void *)contextof(0).get_cs_eip(), " exception: ", contextof(0).exception_index,
			" ex: ", contextof(0).exception, " running: ",
			contextof(0).running, endl, flush;
		foreach(c, contextcount) {
			Context& ctx = contextof(c);
			ctx.setup_qemu_switch();
		}
		return 1;  // Tell QEMU that we will come back to simulate
	}


	W64 tsc_at_end = rdtsc();
	machine->update_stats(stats);
	curr_ptl_machine = null;

	W64 seconds = W64(ticks_to_seconds(tsc_at_end - tsc_at_start));
	stats.elapse_seconds = seconds;
	stringbuf sb;
	sb << endl, "Stopped after ", sim_cycle, " cycles, ", total_user_insns_committed, " instructions and ",
	   seconds, " seconds of sim time (cycle/sec: ", W64(double(sim_cycle) / double(seconds)), " Hz, insns/sec: ", W64(double(total_user_insns_committed) / double(seconds)), ", insns/cyc: ",  double(total_user_insns_committed) / double(sim_cycle), ")", endl;

	ptl_logfile << sb, flush;
	cerr << sb, flush;

	if (config.dumpcode_filename.set()) {
		//    byte insnbuf[256];
		//    PageFaultErrorCode pfec;
		//    Waddr faultaddr;
		//    Waddr rip = contextof(0).eip;
		//    int n = contextof(0).copy_from_user(insnbuf, rip, sizeof(insnbuf), pfec, faultaddr);
		//    ptl_logfile << "Saving ", n, " bytes from rip ", (void*)rip, " to ", config.dumpcode_filename, endl, flush;
		//    ostream(config.dumpcode_filename).write(insnbuf, n);
	}

#ifdef PTLSIM_HYPERVISOR
	last_printed_status_at_ticks = 0;
//	update_progress();
	cerr << endl;
#endif
	print_stats_in_log();

	
	const char *final_name = "final";
    strncpy(stats.snapshot_name, final_name, sizeof(final_name));
	stats.snapshot_uuid = statswriter.next_uuid();
	statswriter.write(&stats, final_name);

	statswriter.close();

	if(config.kill) {
		ptl_logfile << "Received simulation kill signal, stopped the simulation and killing the VM\n";
		ptl_logfile.flush();
		exit(0);
	}

	return 0;
}

extern "C" void update_progress() {
  W64 ticks = rdtsc();
  W64s delta = (ticks - last_printed_status_at_ticks);
  if unlikely (delta < 0) delta = 0;
  if unlikely (delta >= ticks_per_update) {
    double seconds = ticks_to_seconds(delta);
    double cycles_per_sec = (sim_cycle - last_printed_status_at_cycle) / seconds;
    double insns_per_sec = (total_user_insns_committed - last_printed_status_at_user_insn) / seconds;
    
    stringbuf sb;
    sb << "Completed ", intstring(sim_cycle, 13), " cycles, ", intstring(total_user_insns_committed, 13), " commits: ", 
      intstring((W64)cycles_per_sec, 9), " Hz, ", intstring((W64)insns_per_sec, 9), " insns/sec";

    sb << ": rip";
    foreach (i, contextcount) {
      Context& ctx = contextof(i);
#ifdef PTLSIM_HYPERVISOR
      if (!ctx.running) {
//        static const char* runstate_names[] = {"running", "runnable", "blocked", "offline"}; 
//        const char* runstate_name = (inrange(ctx.runstate.state, 0, lengthof(runstate_names)-1)) ? runstate_names[ctx.runstate.state] : "???";

		  static const char* runstate_names[] = {"stopped", "running"};
		  const char* runstate_name = runstate_names[ctx.running];

//        sb << " (", runstate_name, ":",ctx.runstate.state, ")";
        sb << " (", runstate_name, ":",ctx.running, ")";
        if(!sim_cycle){
          ctx.running = 1;
        }
        continue;
      }
#endif
      sb << ' ', hexstring(contextof(i).get_cs_eip(), 64);
    }

    while (sb.size() < 160) sb << ' ';

    ptl_logfile << sb, endl, flush;
    //#ifdef PTLSIM_HYPERVISOR
    cerr << "\r  ", sb, flush;
    //#endif
    last_printed_status_at_ticks = ticks;
    last_printed_status_at_cycle = sim_cycle;
    last_printed_status_at_user_insn = total_user_insns_committed;
  }

  if unlikely ((sim_cycle - last_stats_captured_at_cycle) >= config.snapshot_cycles) {
    last_stats_captured_at_cycle = sim_cycle;
    capture_stats_snapshot();
  }

  if unlikely (config.snapshot_now.set()) {
    capture_stats_snapshot(config.snapshot_now);
    config.snapshot_now.reset();
  }

}



bool simulate(const char* machinename) {
  PTLsimMachine* machine = PTLsimMachine::getmachine(machinename);

  if (!machine) {
    ptl_logfile << "Cannot find core named '", machinename, "'", endl;
    cerr << "Cannot find core named '", machinename, "'", endl;
    return 0;
  }

  if (!machine->initialized) {
    ptl_logfile << "Initializing core '", machinename, "'", endl;
    if (!machine->init(config)) {
      ptl_logfile << "Cannot initialize core model; check its configuration!", endl;
      return 0;
    }
    machine->initialized = 1;
  }

  ptl_logfile << "Switching to simulation core '", machinename, "'...", endl, flush;
  cerr <<  "Switching to simulation core '", machinename, "'...", endl, flush;
  ptl_logfile << "Stopping after ", config.stop_at_user_insns, " commits", endl, flush;
  cerr << "Stopping after ", config.stop_at_user_insns, " commits", endl, flush;

  // Update stats every half second:
  ticks_per_update = seconds_to_ticks(0.2);
  //ticks_per_update = seconds_to_ticks(0.1);
  last_printed_status_at_ticks = 0;
  last_printed_status_at_user_insn = 0;
  last_printed_status_at_cycle = 0;

  W64 tsc_at_start = rdtsc();
  curr_ptl_machine = machine;
  machine->run(config);
  W64 tsc_at_end = rdtsc();
  machine->update_stats(stats);
  curr_ptl_machine = null;

  W64 seconds = W64(ticks_to_seconds(tsc_at_end - tsc_at_start));
  stats.elapse_seconds = seconds;
  stringbuf sb;
  sb << endl, "Stopped after ", sim_cycle, " cycles, ", total_user_insns_committed, " instructions and ",
    seconds, " seconds of sim time (cycle/sec: ", W64(double(sim_cycle) / double(seconds)), " Hz, insns/sec: ", W64(double(total_user_insns_committed) / double(seconds)), ", insns/cyc: ",  double(total_user_insns_committed) / double(sim_cycle), ")", endl;

  ptl_logfile << sb, flush;
  cerr << sb, flush;

  if (config.dumpcode_filename.set()) {
//    byte insnbuf[256];
//    PageFaultErrorCode pfec;
//    Waddr faultaddr;
//    Waddr rip = contextof(0).eip;
//    int n = contextof(0).copy_from_user(insnbuf, rip, sizeof(insnbuf), pfec, faultaddr);
//    ptl_logfile << "Saving ", n, " bytes from rip ", (void*)rip, " to ", config.dumpcode_filename, endl, flush;
//    ostream(config.dumpcode_filename).write(insnbuf, n);
  }

#ifdef PTLSIM_HYPERVISOR
  last_printed_status_at_ticks = 0;
  update_progress();
  cerr << endl;
#endif
  print_stats_in_log();

  return 0;
}

extern void shutdown_uops();

void shutdown_subsystems() {
  //
  // Let the subsystems close any special files or buffers
  // they may have open:
  //
  shutdown_uops();
  shutdown_decode();
  ptl_mm_flush_logging();
}

RIPVirtPhys& RIPVirtPhys::update(Context& ctx, int bytes) {

  use64 = ctx.use64;
  use32 = ctx.use32;
  ss32 = (ctx.hflags >> HF_SS32_SHIFT) & 1;
  hflags = ctx.hflags;
  cs_base = ctx.segs[R_CS].base;
  kernel = ctx.kernel_mode;
  df = ((ctx.internal_eflags & FLAG_DF) != 0);
  padlo = 0;
  padhi = 0;

  int exception = 0;
  PageFaultErrorCode pfec = 0;
  physaddr = ctx.check_and_translate(rip, 0, 0, 0, exception,
		  pfec, true);

  if(exception > 0) {
	  physaddr = INVALID;
  }

//  mfnlo = (invalid) ? INVALID : pte.mfn;
//  mfnhi = mfnlo;
//  if unlikely (invalid) ctx.flush_tlb_virt(rip);

//  int page_crossing = ((lowbits(rip, 12) + (bytes-1)) >> 12);

  //
  // Since table lookups only know the RIP of the target and not
  // its size, we don't know if there is a page crossing. Hence,
  // we always assume there is. BB translation (case above) may
  // be more optimized, only doing this if the pages are truly
  // different.
  //
  //++MTY TODO:
  // If BBs are terminated at the first insn to cross a page,
  // technically we could get away with only checking if the
  // byte at rip + (15-1) would hit the next page.
  //

//  if unlikely (page_crossing) {
//    pte = ctx.virt_to_pte(rip + (bytes-1));
//    invalid = ((!pte.p) | pte.nx | ((!ctx.kernel_mode) & (!pte.us)));
//    mfnhi = (invalid) ? INVALID : pte.mfn;
//    if unlikely (invalid) ctx.flush_tlb_virt(rip + (bytes-1));
//  }

  return *this;
}

Waddr Context::virt_to_pte_phys_addr(W64 rawvirt, int level) {
	ptl_logfile << "call to Context::virt_to_pte_phys_addr, function not implemented\n";
	assert(0);
	return INVALID_PHYSADDR;
}

int Context::copy_from_user(void* target, Waddr source, int bytes, PageFaultErrorCode& pfec, Waddr& faultaddr, bool forexec) {

	if (source == 0) {
		return 0;
	}

	int n = 0 ;
	pfec = 0;

	// Avadh: We use QEMU's ldub_code function to copy all the bytes
	// from the simulating domain, this is slow but it works
	// FIXME : Try to make it like memcpy to copy faster if this gets slower
	this->setup_qemu_switch();

	if(logable(10))
		ptl_logfile << "Copying from userspace ", bytes, " bytes from ",
					source, endl;

	int exception = 0;
	Waddr physaddr = check_and_translate(source, 0, 0, 0, exception,
			pfec, 1);
	if (exception) {
		int old_exception = exception_index;
		int mmu_index = cpu_mmu_index((CPUState*)this);
		int fail = cpu_x86_handle_mmu_fault((CPUX86State*)this,
				source, 2, mmu_index, 1);
		if(logable(10))
			ptl_logfile << "page fault while reading code fault:", fail, 
						" source_addr:", (void*)(source), 
						" eip:", (void*)(eip), endl;
//		if (fail && (this->eip == (W64)(source))) {
//
//			if(logable(10))
//				ptl_logfile << "Will handle page fault in QEMU\n";
//			assert(eip == source);
//			char d = ldub_code((target_ulong)(source));
//			// if we retured from above function means that
//			// the page fault in handled without exception
//			// so now we have a valid tlb entry so fetch the code
//			fail = 0;
//		}
		if (fail) {
			if(logable(10))
				ptl_logfile << "Unable to read code from ", 
							hexstring(source, 64), endl;
			setup_ptlsim_switch();
			// restore the exception index as it will be 
			// restore when we try to commit this entry from ROB
			exception_index = old_exception;
			return -1;
		}
	}
	
//	byte* source_b = (byte*)(source);
	target_ulong source_b = source;
	byte* target_b = (byte*)(target);
	while(n < bytes) {
		char data = ldub_code(source_b);
		if(logable(109)) {
			ptl_logfile << "[", hexstring((W8)(data), 8),
						"-", hexstring((W8)(ldub_code(source_b)), 8), 
						"@", (void*)(source_b), "] ";
		}
		target_b[n] = data;
		source_b++;
		n++;
	}

	if(logable(109)) ptl_logfile << endl;

	setup_ptlsim_switch();

	if(logable(10))
		ptl_logfile << "Copy done..\n";

	return n;
	//FIXME: We have to check the page permission of both source and
	//target to make sure that userspace can't copy from kernel mode

//	int exception = 0;
//	Waddr source_paddr = check_and_translate(source, 0, false, false, exception, pfec, 1);
//
//	if(exception > 0 || pfec > 0) {
//		this->exception = exception;
//		faultaddr = source;
//		return 0;
//	}
//	n = min(4096 - lowbits(source, 12), (Waddr)bytes);
//	memcpy(target, (void*)source_paddr, n);
//
//	// Check if all the bytes are read in first page or not
//	if likely (n == bytes) return n;
//
//	source_paddr = check_and_translate(source + n, 0, false, false, exception, pfec, 1);
//	if(exception > 0 || pfec > 0) {
//		this->exception = exception;
//		faultaddr = source + n;
//		return 0;
//	}
//
//	Waddr next_page_addr = ((source >> TARGET_PAGE_BITS) + 1) << TARGET_PAGE_BITS;
//
//	memcpy((byte*)target + n, (void*)next_page_addr, bytes - n);
//	n = bytes;
//	return n;
}

void Context::update_mode_count() {
	W64 prev_cycles = cycles_at_last_mode_switch;
	W64 prev_insns = insns_at_last_mode_switch;
	W64 delta_cycles = sim_cycle - cycles_at_last_mode_switch;
	W64 delta_insns = total_user_insns_committed - 
		insns_at_last_mode_switch;

	cycles_at_last_mode_switch = sim_cycle;
	insns_at_last_mode_switch = total_user_insns_committed;

	if likely (use64) {
		if(kernel_mode) {
			per_core_event_update(cpu_index, cycles_in_mode.kernel64 += delta_cycles);
			per_core_event_update(cpu_index, insns_in_mode.kernel64 += delta_insns);
		} else {
			per_core_event_update(cpu_index, cycles_in_mode.user64 += delta_cycles);
			per_core_event_update(cpu_index, insns_in_mode.user64 += delta_insns);
		}
	} else {
		if(kernel_mode) {
			per_core_event_update(cpu_index, cycles_in_mode.kernel32 += delta_cycles);
			per_core_event_update(cpu_index, insns_in_mode.kernel32 += delta_insns);
		} else {
			per_core_event_update(cpu_index, cycles_in_mode.user32 += delta_cycles);
			per_core_event_update(cpu_index, insns_in_mode.user32 += delta_insns);
		}
	}
}

Waddr Context::check_and_translate(Waddr virtaddr, int sizeshift, bool store, bool internal, int& exception, PageFaultErrorCode& pfec, bool is_code) {

	exception = 0;
	pfec = 0;

//	if unlikely (lowbits(virtaddr, sizeshift)) {
//		exception = EXCEPTION_UnalignedAccess;
//		return INVALID_PHYSADDR;
//	}

	if unlikely (internal) {
		//
		// Directly mapped to PTL space (microcode load/store)
		// We need to patch in PTLSIM_VIRT_BASE since in 32-bit
		// mode, ctx.virt_addr_mask will chop off these bits.
		//
		// Now in QEMU we dont have any special mapping of this 
		// memory region, so virtualaddress is where we
		// will store the internal data
		//
		return virtaddr;
	}

	// TODO : We are currently looking directly at the TLB of QEMU
	// we don't simulate multilevel page entries right now.
	bool page_not_present;
	bool page_read_only;
	bool page_kernel_only;

	int mmu_index = cpu_mmu_index((CPUState*)this);
	int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
	W64 tlb_addr;
redo:
	if likely (!store) {
		if likely (!is_code) {
			tlb_addr = tlb_table[mmu_index][index].addr_read;
		} else {
			tlb_addr = tlb_table[mmu_index][index].addr_code;
		}
	} else {
		tlb_addr = tlb_table[mmu_index][index].addr_write;
	}

	if(logable(10)) {
		ptl_logfile << "mmu_index:", mmu_index, " index:", index,
					" virtaddr:", hexstring(virtaddr, 64), 
					" tlb_addr:", hexstring(tlb_addr, 64), 
					" virtpage:", hexstring(
							(virtaddr & TARGET_PAGE_MASK), 64), 
					" tlbpage:", hexstring(
							(tlb_addr & TARGET_PAGE_MASK), 64),
					" addend:", hexstring(
							tlb_table[mmu_index][index].addend, 64),
					endl;
	}
	if likely ((virtaddr & TARGET_PAGE_MASK) == 
			(tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
		// Check if its not MMIO address
		if(tlb_addr & ~TARGET_PAGE_MASK) {
//			cerr << "Doing MMIO Access at address : ", 
//				hexstring(virtaddr, 64), endl;
			return (Waddr)(virtaddr + iotlb[mmu_index][index]);
		}

		// we find valid TLB entry, return the physical address for it
		return (Waddr)(virtaddr + tlb_table[mmu_index][index].addend);
	}

	// In QEMU we have to call helper function to handle Page faults
	// FIXME : Currently we are simply calling QEMU functions to 
	// handle the faults but we should implement a way to simulate
	// the dealys for page faults
//	tlb_fill(virtaddr, store, mmu_index, null);
//	goto redo;

	// Can't find valid TLB entry, its an exception
	exception = (store) ? EXCEPTION_PageFaultOnWrite : EXCEPTION_PageFaultOnRead;
	pfec.rw = store;
	pfec.us = (!kernel_mode);

	//
	// Flush out the bogus TLB entry to avoid an infinite loop after the
	// kernel updates the page tables. Technically only valid PTEs should
	// be cached in the TLB, but we don't know what "valid" means until
	// we do the full protection checks. Hence we just invalidate it here:
	//
	//flush_tlb_virt(virtaddr);
	//
	// Avadh: TODO and also needs to check that in QEMU do we need to flush
	// the TLB?
	return INVALID_PHYSADDR;
}

bool Context::is_mmio_addr(Waddr virtaddr, bool store) {
	
	int mmu_index = cpu_mmu_index((CPUState*)this);
	int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
	W64 tlb_addr;

	if likely (!store) {
		tlb_addr = tlb_table[mmu_index][index].addr_read;
	} else {
		tlb_addr = tlb_table[mmu_index][index].addr_write;
	}

	if(logable(10)) {
		ptl_logfile << "mmio mmu_index:", mmu_index, " index:", index,
					" virtaddr:", hexstring(virtaddr, 64), 
					" tlb_addr:", hexstring(tlb_addr, 64), 
					" virtpage:", hexstring(
							(virtaddr & TARGET_PAGE_MASK), 64), 
					" tlbpage:", hexstring(
							(tlb_addr & TARGET_PAGE_MASK), 64),
					" addend:", hexstring(
							tlb_table[mmu_index][index].addend, 64),
					endl;
	}
	if likely ((virtaddr & TARGET_PAGE_MASK) == 
			(tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
		// Check if its not MMIO address
		if(tlb_addr & ~TARGET_PAGE_MASK) {
			return true;
		}
	}

	return false;

}

int copy_from_user_phys_prechecked(void* target, Waddr source, int bytes, Waddr& faultaddr) {

	int n = 0 ;

	//FIXME: We have to check the page permission of both source and
	//target to make sure that userspace can't copy from kernel mode

	int exception;
	PageFaultErrorCode pfec;
	Waddr source_paddr = contextof(0).check_and_translate(source, 0, false, false, exception, pfec);

	if(exception > 0 || pfec > 0) {
		faultaddr = source;
		return 0;
	}
	n = min(4096 - lowbits(source, 12), (Waddr)bytes);
	memcpy(target, (void*)source_paddr, n);

	// Check if all the bytes are read in first page or not
	if likely (n == bytes) return n;

	source_paddr = contextof(0).check_and_translate(source + n, 0, false, false, exception, pfec);
	if(exception > 0 || pfec > 0) {
		faultaddr = source + n;
		return 0;
	}

	Waddr next_page_addr = ((source >> TARGET_PAGE_BITS) + 1) << TARGET_PAGE_BITS;

	memcpy((byte*)target + n, (void*)next_page_addr, bytes - n);
	n = bytes;
	return n;
}

void Context::propagate_x86_exception(byte exception, W32 errorcode , Waddr virtaddr ) {
	ptl_logfile << "Propagating exception from simulation at eip: ",
				this->eip, " cycle: ", sim_cycle, endl;
	setup_qemu_switch();
	if(errorcode) {
		raise_exception_err((int)exception, (int)errorcode);
	} else {
		raise_exception((int)exception);
	}
	setup_ptlsim_switch();
}

W64 Context::loadvirt(Waddr virtaddr, int sizeshift) {
	Waddr addr = virtaddr; //floor(virtaddr, 8);
	assert(virtaddr > 0xffff);
	setup_qemu_switch();
	W64 data = 0;

	if(is_mmio_addr(virtaddr, 0)) {
		switch(sizeshift) {
			case 0: {
						W8 d = ldub_kernel(virtaddr);
						data = (W64)d;
						break;
					}
			case 1: {
						W16 d = lduw_kernel(virtaddr);
						data = (W64)d;
						break;
					}
			case 2: {
						W32 d = ldl_kernel(virtaddr);
						data = (W64)d;
						break;
					}
			default:
					data = ldq_kernel(virtaddr);
		}
		if(logable(10))
			ptl_logfile << "MMIO READ addr: ", hexstring(virtaddr, 64),
						" data: ", hexstring(data, 64), " size: ",
						sizeshift, endl;
//		cerr << "MMIO READ addr: ", hexstring(virtaddr, 64),
//					" data: ", hexstring(data, 64), " size: ",
//					sizeshift, endl;
		return data;
	}

	if(kernel_mode) {
		data = ldq_kernel(addr);
	} else {
		data = ldq_user(addr);
	}
	if(logable(10))
		ptl_logfile << "Context::loadvirt addr[", hexstring(addr, 64),
					"] data[", hexstring(data, 64), "] origaddr[",
					hexstring(virtaddr, 64), "]\n";
	setup_ptlsim_switch();
	return data;
}

W64 Context::loadphys(Waddr addr, bool internal, int sizeshift) {
	// Currently we check sizeshift only for internal data
	// for data on RAM or IO we load data at 64 bit boundry
	if(internal) {
		W64 data = 0;
		switch(sizeshift) {
			case 0: { // load one byte
				byte b = byte(*(byte*)(addr));
				data = b;
				break;
					}
			case 1: { // load a word
				W16 w = W16(*(W16*)(addr));
				data = w;
				break;
					}
			case 2: { // load double word
				W32 dw = W32(*(W32*)(addr));
				data = dw;
				break;
					}
			case 3: // load quad word
			default: {
				data = W64(*(W64*)(addr));
					 }
		}

		if(logable(10))
			ptl_logfile << "Context::internal_loadphys addr[", 
						hexstring(addr, 64), "] data[", 
						hexstring(data, 64), "] sizeshift[", sizeshift,
						"] ", endl;
		return data;
	}

	W64 data = 0;
	Waddr orig_addr = addr;
	addr = floor(addr, 8);
	setup_qemu_switch();
	data = ldq_raw((uint8_t*)addr);

	if(logable(10))
		ptl_logfile << "Context::loadphys addr[", hexstring(addr, 64),
					"] data[", hexstring(data, 64), "] origaddr[",
					hexstring(orig_addr, 64), "]\n";
	setup_ptlsim_switch();
	return data;
}

//W64 Context::storemask_virt(Waddr virtaddr, W64 data, int size) {
//	switch(size) {
//		case 0: // byte write
//			(kernel_mode) ? stb_kernel(virtaddr, data) : 
//				stb_user(virtaddr, data);
//			break;
//		case 1: // word write
//			(kernel_mode) ? stw_kernel(virtaddr, data) :
//				stw_user(virtaddr, data);
//			break;
//		case 2: // double word write
//			(kernel_mode) ? stl_kernel(virtaddr, data) :
//				stl_user(virtaddr, data);
//			break;
//		case 3: // quad word write
//		default:
//			(kernel_mode) ? stq_kernel(virtaddr, data) :
//				stq_user(virtaddr, data);
//			break;
//	}
//	return data;
//}

W64 Context::storemask_virt(Waddr virtaddr, W64 data, byte bytemask, int sizeshift) {
	W64 old_data = 0;
	setup_qemu_switch();
	Waddr paddr = floor(virtaddr, 8);

	if(logable(10))
		ptl_logfile << "Trying to write to addr: ", hexstring(paddr, 64),
					" with bytemask ", bytemask, " data: ", hexstring(
							data, 64), endl;

	if(is_mmio_addr(virtaddr, 1)) {
		switch(sizeshift) {
			case 0: {
						stb_kernel(virtaddr, (W8)data);
						break;
					}
			case 1: {
						stw_kernel(virtaddr, (W16)data);
						break;
					}
			case 2: {
						stl_kernel(virtaddr, (W32)data);
						break;
					}
			default:
					stq_kernel(virtaddr, data);
		}
		if(logable(10))
			ptl_logfile << "MMIO WRITE addr: ", hexstring(virtaddr, 64),
						" data: ", hexstring(data, 64), " size: ",
						sizeshift, endl;
//		cerr << "MMIO WRITE addr: ", hexstring(virtaddr, 64),
//					" data: ", hexstring(data, 64), " size: ",
//					sizeshift, endl;
		return data;
	}

	switch(sizeshift) {
		case 0: // byte write
			(kernel_mode) ? stb_kernel(virtaddr, data) : 
				stb_user(virtaddr, data);
			break;
		case 1: // word write
			(kernel_mode) ? stw_kernel(virtaddr, data) :
				stw_user(virtaddr, data);
			break;
		case 2: // double word write
			(kernel_mode) ? stl_kernel(virtaddr, data) :
				stl_user(virtaddr, data);
			break;
		case 3: // quad word write
		default:
			(kernel_mode) ? stq_kernel(virtaddr, data) :
				stq_user(virtaddr, data);
			break;
	}
	if(logable(10))
		ptl_logfile << "Context::storemask addr[", hexstring(paddr, 64),
					"] data[", hexstring(data, 64), "]\n";
	return data;

//	if(kernel_mode) {
//		old_data = ldq_kernel(paddr);
//	} else {
//		old_data = ldq_user(paddr);
//	}
//	W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask], old_data, data);
//	ptl_logfile << "Context::storemask addr[", hexstring(paddr, 64),
//				"] data[", hexstring(merged_data, 64), "]\n";
//	if(kernel_mode) {
//		stq_kernel(paddr, merged_data);
//	} else {
//		stq_user(paddr, merged_data);
//	}
//#define CHECK_STORE
#ifdef CHECK_STORE
	W64 new_data = 0;
	if(kernel_mode) {
		new_data = ldq_kernel(paddr);
	} else {
		new_data = ldq_user(paddr);
	}
	if(logable(10))
		ptl_logfile << "Context::storemask store-check: addr[",
					hexstring(paddr, 64), "] data[", hexstring(new_data,
							64), "]\n";
	assert(new_data == merged_data);
#endif
//	return merged_data;
}

W64 Context::store_internal(Waddr addr, W64 data, byte bytemask) {
	W64 old_data = W64(*(W64*)(addr));
	W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask], 
			old_data, data);
	if(logable(10))
		ptl_logfile << "Context::store_internal addr[", 
					hexstring(addr, 64), "] old_data[", 
					hexstring(old_data, 64), "] new_data[",
					hexstring(merged_data, 64), "]\n";
	*(W64*)(addr) = merged_data;
	return merged_data;
}

W64 Context::storemask(Waddr paddr, W64 data, byte bytemask) {
	W64 old_data = 0;
	setup_qemu_switch();
	if(logable(10))
		ptl_logfile << "Trying to write to addr: ", hexstring(paddr, 64),
					" with bytemask ", bytemask, " data: ", hexstring(
							data, 64), endl;
	old_data = ldq_raw((uint8_t*)paddr);
	W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask], old_data, data);
	if(logable(10))
		ptl_logfile << "Context::storemask addr[", hexstring(paddr, 64),
					"] data[", hexstring(merged_data, 64), "]\n";
	stq_raw((uint8_t*)paddr, merged_data);
#define CHECK_STORE
#ifdef CHECK_STORE
	W64 new_data = 0;
	new_data = ldq_raw((uint8_t*)paddr);
	ptl_logfile << "Context::storemask store-check: addr[",
				hexstring(paddr, 64), "] data[", hexstring(new_data,
						64), "]\n";
	assert(new_data == merged_data);
#endif
	return data;
}

void Context::handle_page_fault(Waddr virtaddr, int is_write) {
	setup_qemu_switch();
	if(kernel_mode) {
//		cerr << "Page fault in kernel mode...", endl, flush;
		ptl_logfile << "Page fault in kernel mode...", endl, flush;
	}
	if(logable(5)) {
		ptl_logfile << "Context before page fault handling:\n", *this, endl;
	}
	exception_is_int = 0;
	int mmu_index = cpu_mmu_index((CPUState*)this);
	tlb_fill(virtaddr, is_write, mmu_index, null);
	if(kernel_mode) {
//		cerr << "Page fault in kernel mode...handled", endl, flush;
		ptl_logfile << "Page fault in kernel mode...handled", endl, 
					flush;
	}
	setup_ptlsim_switch();
	return;
}

bool Context::try_handle_fault(Waddr virtaddr, bool store) {

	setup_qemu_switch();

	if(logable(10))
		ptl_logfile << "Trying to fill tlb for addr: ", (void*)virtaddr, endl;

	int mmu_index = cpu_mmu_index((CPUState*)this);
	int fault = cpu_x86_handle_mmu_fault((CPUState*)this, virtaddr, store, mmu_index, 1);

	setup_ptlsim_switch();
	if(fault) {
		if(logable(10))
			ptl_logfile << "Fault for addr: ", (void*)virtaddr, endl, flush;

		error_code = 0;
		exception_index = -1;

		return false;
	}

	if(logable(10))
		ptl_logfile << "Tlb fill for addr: ", (void*)virtaddr, endl, flush;

	return true;
}

#endif // CONFIG_ONLY
