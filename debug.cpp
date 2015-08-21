#include <Windows.h>
#include <streambuf>
#include <fstream>
#include <iostream>
#include <ida.hpp>
#include <idd.hpp>
#include <dbg.hpp>
#include <diskio.hpp>
#include <auto.hpp>
#include <funcs.hpp>

#undef interface

#include "emu.h"
#include "debugcpu.h"
#include "debugcon.h"

#include "mess_debmod.h"
#include "nargv.h"

#include "registers.h"

extern int utf8_main(int argc, char *argv[]);

codemap_t g_codemap;
running_machine *g_running_machine = NULL;
eventlist_t g_events;

static bool stopped;
static qthread_t mess_thread;

#define CHECK_FOR_START(x) {if (stopped) return x;}
#define RC_GENERAL 1

static char *register_str_t[] = {
	"d0",
	"d1",
	"d2",
	"d3",
	"d4",
	"d5",
	"d6",
	"d7",

	"a0",
	"a1",
	"a2",
	"a3",
	"a4",
	"a5",
	"a6",
	"a7",

	"pc",
	"sp",
	"isp",
	"usp",

	"curflags",
};

static const char *const SRReg[] =
{
	"C",
	"V",
	"Z",
	"N",
	"X",
	NULL,
	NULL,
	NULL,
	"I",
	"I",
	"I",
	NULL,
	NULL,
	"S",
	NULL,
	"T"
};

register_info_t registers[] =
{
	{ "D0", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D1", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D2", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D3", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D4", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D5", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D6", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "D7", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },

	{ "A0", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A1", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A2", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A3", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A4", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A5", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A6", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "A7", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },

	{ "PC", REGISTER_ADDRESS | REGISTER_IP, RC_GENERAL, dt_dword, NULL, 0 },
	{ "SP", REGISTER_ADDRESS | REGISTER_SP, RC_GENERAL, dt_dword, NULL, 0 },
	{ "ISP", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },
	{ "USP", REGISTER_ADDRESS, RC_GENERAL, dt_dword, NULL, 0 },

	{ "SR", NULL, RC_GENERAL, dt_word, SRReg, 0xFFFF },
};

static const char *register_classes[] =
{
	"General registers",
	NULL
};

static void wait_for_machine()
{
	while (!g_running_machine)
		qsleep(1);
}

static running_machine *get_running_machine()
{
	wait_for_machine();
	return g_running_machine;
}

static address_space *get_addr_space()
{
	wait_for_machine();
	return &get_running_machine()->firstcpu->space();
}

static device_debug *get_debugger()
{
	return get_running_machine()->firstcpu->debug();
}

static symbol_table *get_symbol_table()
{
	return &get_debugger()->symtable();
}

static ea_t get_reg_value(register_t idx)
{
	return get_symbol_table()->value(register_str_t[idx]);
}

static void set_reg_value(register_t idx, const regval_t *value)
{
	get_symbol_table()->set_value(register_str_t[idx], value->ival);
}

static ea_t get_current_pc()
{
	return get_reg_value(R_PC);
}

std::streambuf *CinBuffer, *CoutBuffer, *CerrBuffer;
std::fstream ConsoleInput, ConsoleOutput, ConsoleError;

static void create_console()
{
	// Create a new console window.
	if (!AllocConsole()) return;

	SetConsoleTitle("MESS Console");

	CinBuffer = std::cin.rdbuf();
	CoutBuffer = std::cout.rdbuf();
	CerrBuffer = std::cerr.rdbuf();
	ConsoleInput.open("CONIN$", std::ios::in);
	ConsoleOutput.open("CONOUT$", std::ios::out);
	ConsoleError.open("CONOUT$", std::ios::out);
	std::cin.rdbuf(ConsoleInput.rdbuf());
	std::cout.rdbuf(ConsoleOutput.rdbuf());
	std::cerr.rdbuf(ConsoleError.rdbuf());
}

static void close_console()
{
	ConsoleInput.close();
	ConsoleOutput.close();
	ConsoleError.close();
	std::cin.rdbuf(CinBuffer);
	std::cout.rdbuf(CoutBuffer);
	std::cerr.rdbuf(CerrBuffer);
	CinBuffer = NULL;
	CoutBuffer = NULL;
	CerrBuffer = NULL;

	FreeConsole();
}

static void prepare_codemap()
{
	g_codemap.resize(MAX_ROM_SIZE);
	for (size_t i = 0; i < MAX_ROM_SIZE; ++i)
	{
		g_codemap[i] = std::pair<ea_t, bool>(BADADDR, false);
	}
}

static void apply_codemap()
{
	msg("Applying codemap...\n");
	for (size_t i = 0; i < MAX_ROM_SIZE; ++i)
	{
		std::pair<ea_t, bool> _pair = g_codemap[i];
		if (_pair.second && _pair.first)
		{
			auto_make_code((ea_t)i);
			noUsed((ea_t)i);
		}
		showAddr((ea_t)i);
	}
	noUsed(0, MAX_ROM_SIZE);

	for (size_t i = 0; i < MAX_ROM_SIZE; ++i)
	{
		std::pair<ea_t, bool> _pair = g_codemap[i];
		if (_pair.second && _pair.first && !get_func((ea_t)i))
		{
			add_func(i, BADADDR);
			add_cref(_pair.first, i, fl_CN);
			noUsed((ea_t)i);
		}
		showAddr((ea_t)i);
	}
	noUsed(0, MAX_ROM_SIZE);
	msg("Codemap applied.\n");
}

static bool idaapi execute_mess_cmd(void *ud)
{
	const char *exec = askstr(HIST_CMD, "", "Enter debugger command here:\n");

	if (!exec) return false;

	CMDERR _error = debug_console_execute_command(*g_running_machine, exec, TRUE);

	return (_error == CMDERR_NONE);
}

static int req_id = 0;
bool mess_menu_added = false;
#define MESS_MENU_RUN_CMD "Execute MESS command..."

//---------------------------------------------------------------------------
void remove_mess_menu()
{
	if (mess_menu_added)
		del_menu_item("Debugger/" MESS_MENU_RUN_CMD);
	else
		cancel_exec_request(req_id);
	mess_menu_added = false;
	req_id = 0;
}

//---------------------------------------------------------------------------
void install_mess_menu()
{
	// HACK: We queue this request because commdbg apparently enables the debug menus
	//       just after calling init_debugger().
	struct uireq_install_menu_t : public ui_request_t
	{
		virtual bool idaapi run()
		{
			if (!mess_menu_added)
			{
				mess_menu_added = add_menu_item("Debugger/StepInto",
					MESS_MENU_RUN_CMD,
					NULL,
					SETMENU_INS | SETMENU_CTXAPP,
					execute_mess_cmd,
					NULL);
				enable_menu_item("Debugger/" MESS_MENU_RUN_CMD, false);
			}
			return false;
		}
	};
	req_id = execute_ui_requests(new uireq_install_menu_t, NULL);
}

static void pause_execution()
{
	get_debugger()->halt_on_next_instruction("");
	qsleep(10);
}

static void continue_execution()
{
	get_debugger()->go();
}

static void finish_execution()
{
	qthread_join(mess_thread);
	qthread_kill(mess_thread);
	g_running_machine = NULL;
	stopped = true;
	apply_codemap();
}

// Initialize debugger
// Returns true-success
// This function is called from the main thread
static bool idaapi init_debugger(const char *hostname,
	int port_num,
	const char *password)
{
	install_mess_menu();
	create_console();
	set_process_options(NULL, "genesis", NULL, NULL, NULL, 0);
	SetCurrentDirectoryA(idadir("plugins"));
	return true;
}

// Terminate debugger
// Returns true-success
// This function is called from the main thread
static bool idaapi term_debugger(void)
{
	enable_menu_item("Debugger/" MESS_MENU_RUN_CMD, false);
	close_console();
	return true;
}

// Return information about the n-th "compatible" running process.
// If n is 0, the processes list is reinitialized.
// 1-ok, 0-failed, -1-network error
// This function is called from the main thread
static int idaapi process_get_info(int n, process_info_t *info)
{
	return 0;
}

static void GetPluginName(char *szModule)
{
	MEMORY_BASIC_INFORMATION mbi;
	SetLastError(ERROR_SUCCESS);
	VirtualQuery(GetPluginName, &mbi, sizeof(mbi));

	GetModuleFileNameA((HINSTANCE)mbi.AllocationBase, (LPSTR)szModule, 2048);
}

static int idaapi mess_process(void *ud)
{
	int rc;

	NARGV *params = (NARGV *)ud;
	rc = utf8_main(params->argc, params->argv);
	nargv_free(params);

	debug_event_t ev;
	ev.eid = PROCESS_EXIT;
	ev.pid = 1;
	ev.handled = true;
	ev.exit_code = rc;

	g_events.enqueue(ev, IN_BACK);

	return rc;
}

// Start an executable to debug
// 1 - ok, 0 - failed, -2 - file not found (ask for process options)
// 1|CRC32_MISMATCH - ok, but the input file crc does not match
// -1 - network error
// This function is called from debthread
static int idaapi start_process(const char *path,
	const char *args,
	const char *startdir,
	int dbg_proc_flags,
	const char *input_path,
	uint32 input_file_crc32)
{
	enable_menu_item("Debugger/" MESS_MENU_RUN_CMD, true);

	char szModule[MAX_PATH];
	GetPluginName(szModule);

	char cmdline[2048];
	qsnprintf(cmdline, sizeof(cmdline), "\"%s\" %s -debug -cart \'\"%s\"\'", szModule, args, path);

	stopped = false;
	prepare_codemap();
	NARGV *params = nargv_parse(cmdline);
	mess_thread = qthread_create(mess_process, params);

	debug_event_t ev;
	ev.eid = PROCESS_START;
	ev.pid = 1;
	ev.tid = 1;
	ev.ea = BADADDR;
	ev.handled = true;

	qstrncpy(ev.modinfo.name, path, sizeof(ev.modinfo.name));
	ev.modinfo.base = 0;
	ev.modinfo.size = 0;
	ev.modinfo.rebase_to = BADADDR;

	g_events.enqueue(ev, IN_BACK);

	ev.eid = PROCESS_SUSPEND;
	ev.pid = 1;
	ev.tid = 1;
	ev.ea = BADADDR;
	ev.handled = true;

	g_events.enqueue(ev, IN_BACK);

	return 1;
}

// rebase database if the debugged program has been rebased by the system
// This function is called from the main thread
static void idaapi rebase_if_required_to(ea_t new_base)
{
}

// Prepare to pause the process
// This function will prepare to pause the process
// Normally the next get_debug_event() will pause the process
// If the process is sleeping then the pause will not occur
// until the process wakes up. The interface should take care of
// this situation.
// If this function is absent, then it won't be possible to pause the program
// 1-ok, 0-failed, -1-network error
// This function is called from debthread
static int idaapi prepare_to_pause_process(void)
{
	debug_event_t ev;
	ev.eid = PROCESS_SUSPEND;
	ev.pid = 1;
	ev.tid = 1;
	ev.ea = get_current_pc();
	ev.handled = true;

	g_events.enqueue(ev, IN_BACK);

	return 1;
}

// Stop the process.
// May be called while the process is running or suspended.
// Must terminate the process in any case.
// The kernel will repeatedly call get_debug_event() and until PROCESS_EXIT.
// In this mode, all other events will be automatically handled and process will be resumed.
// 1-ok, 0-failed, -1-network error
// This function is called from debthread
static int idaapi mess_exit_process(void)
{
	CHECK_FOR_START(1);
	continue_execution();
	get_running_machine()->schedule_exit();

	return 1;
}

// Get a pending debug event and suspend the process
// This function will be called regularly by IDA.
// This function is called from debthread
static gdecode_t idaapi get_debug_event(debug_event_t *event, int timeout_ms)
{
	while (true)
	{
		// are there any pending events?
		if (g_events.retrieve(event))
		{
			if (event->eid != PROCESS_EXIT)
				pause_execution();
			return g_events.empty() ? GDE_ONE_EVENT : GDE_MANY_EVENTS;
		}
		if (g_events.empty())
			break;
	}
	return GDE_NO_EVENT;
}

// Continue after handling the event
// 1-ok, 0-failed, -1-network error
// This function is called from debthread
static int idaapi continue_after_event(const debug_event_t *event)
{
	switch (event->eid)
	{
	case BREAKPOINT:
	case STEP:
	case PROCESS_SUSPEND:
	{
		switch (get_running_notification())
		{
		case dbg_null:
			continue_execution();
			break;

		case dbg_run_to:
		{
			ea_t addr = get_screen_ea();
			get_debugger()->go(addr);
		} break;
		}
	} break;
	case PROCESS_EXIT:
		finish_execution();
		break;
	}

	return 1;
}

// The following function will be called by the kernel each time
// when it has stopped the debugger process for some reason,
// refreshed the database and the screen.
// The debugger module may add information to the database if it wants.
// The reason for introducing this function is that when an event line
// LOAD_DLL happens, the database does not reflect the memory state yet
// and therefore we can't add information about the dll into the database
// in the get_debug_event() function.
// Only when the kernel has adjusted the database we can do it.
// Example: for imported PE DLLs we will add the exported function
// names to the database.
// This function pointer may be absent, i.e. NULL.
// This function is called from the main thread
static void idaapi stopped_at_debug_event(bool dlls_added)
{
}

// The following functions manipulate threads.
// 1-ok, 0-failed, -1-network error
// These functions are called from debthread
static int idaapi thread_suspend(thid_t tid) // Suspend a running thread
{
	return 0;
}

static int idaapi thread_continue(thid_t tid) // Resume a suspended thread
{
	return 0;
}

static int do_step(dbg_notification_t idx)
{
	switch (idx)
	{
	case dbg_step_into:
		get_debugger()->single_step();
		break;

	case dbg_step_over:
		get_debugger()->single_step_over();
		break;

	case dbg_step_until_ret:
		get_debugger()->single_step_out();
		break;

	default:
		return 0;
	}

	return 1;
}

static int idaapi thread_set_step(thid_t tid) // Run one instruction in the thread
{
	return do_step(get_running_notification());
}

// Read thread registers
//    tid    - thread id
//    clsmask- bitmask of register classes to read
//    regval - pointer to vector of regvals for all registers
//             regval is assumed to have debugger_t::registers_size elements
// 1-ok, 0-failed, -1-network error
// This function is called from debthread
static int idaapi read_registers(thid_t tid, int clsmask, regval_t *values)
{
	if (clsmask & RC_GENERAL)
	{
		values[R_D0].ival = get_reg_value(R_D0);
		values[R_D1].ival = get_reg_value(R_D1);
		values[R_D2].ival = get_reg_value(R_D2);
		values[R_D3].ival = get_reg_value(R_D3);
		values[R_D4].ival = get_reg_value(R_D4);
		values[R_D5].ival = get_reg_value(R_D5);
		values[R_D6].ival = get_reg_value(R_D6);
		values[R_D7].ival = get_reg_value(R_D7);

		values[R_A0].ival = get_reg_value(R_A0);
		values[R_A1].ival = get_reg_value(R_A1);
		values[R_A2].ival = get_reg_value(R_A2);
		values[R_A3].ival = get_reg_value(R_A3);
		values[R_A4].ival = get_reg_value(R_A4);
		values[R_A5].ival = get_reg_value(R_A5);
		values[R_A6].ival = get_reg_value(R_A6);
		values[R_A7].ival = get_reg_value(R_A7);

		values[R_PC].ival = get_reg_value(R_PC);
		values[R_SP].ival = get_reg_value(R_SP);
		values[R_ISP].ival = get_reg_value(R_ISP);
		values[R_USP].ival = get_reg_value(R_USP);
		values[R_SR].ival = get_reg_value(R_SR);
	}

	return 1;
}

// Write one thread register
//    tid    - thread id
//    regidx - register index
//    regval - new value of the register
// 1-ok, 0-failed, -1-network error
// This function is called from debthread
static int idaapi write_register(thid_t tid, int regidx, const regval_t *value)
{
	set_reg_value((register_t)regidx, value);
	return 1;
}

//
// The following functions manipulate bytes in the memory.
//
// Get information on the memory areas
// The debugger module fills 'areas'. The returned vector MUST be sorted.
// Returns:
//   -3: use idb segmentation
//   -2: no changes
//   -1: the process does not exist anymore
//    0: failed
//    1: new memory layout is returned
// This function is called from debthread
static int idaapi get_memory_info(meminfo_vec_t &areas)
{
	return -3;
}

static size_t mess_memory_read(ea_t ea, void *buffer, size_t size)
{
	for (size_t i = 0; i < size; ++i)
	{
		((UINT8*)buffer)[i] = get_addr_space()->read_byte(ea + i);
	}
	return size;
}

// Read process memory
// Returns number of read bytes
// 0 means read error
// -1 means that the process does not exist anymore
// This function is called from debthread
static ssize_t idaapi read_memory(ea_t ea, void *buffer, size_t size)
{
	CHECK_FOR_START(0);
	return mess_memory_read(ea, buffer, size);
}

static size_t mess_memory_write(ea_t ea, const void *buffer, size_t size)
{
	for (size_t i = 0; i < size; ++i)
	{
		get_addr_space()->write_byte(ea + i, ((UINT8*)buffer)[i]);
	}
	return size;
}

// Write process memory
// Returns number of written bytes, -1-fatal error
// This function is called from debthread
static ssize_t idaapi write_memory(ea_t ea, const void *buffer, size_t size)
{
	return mess_memory_write(ea, buffer, size);
}

// Is it possible to set breakpoint?
// Returns: BPT_...
// This function is called from debthread or from the main thread if debthread
// is not running yet.
// It is called to verify hardware breakpoints.
static int idaapi is_ok_bpt(bpttype_t type, ea_t ea, int len)
{
	if (ea % 2 != 0)
		return BPT_BAD_ALIGN;

	switch (type)
	{
		//case BPT_SOFT:
	case BPT_EXEC:
	case BPT_READ: // there is no such constant in sdk61
	case BPT_WRITE:
	case BPT_RDWR:
		return BPT_OK;
	default:
		return BPT_BAD_TYPE;
	}
}

static int get_bpt_index(ea_t ea)
{
	for (device_debug::breakpoint *bp = get_debugger()->breakpoint_first(); bp != NULL; bp = bp->next())
		if (bp->address() == ea)
			return bp->index();
	return -1;
}

static int get_wpt_index(ea_t ea)
{
	for (device_debug::watchpoint *wp = get_debugger()->watchpoint_first(AS_PROGRAM); wp != NULL; wp = wp->next())
		if (wp->address() == ea)
			return wp->index();
	return -1;
}

// Add/del breakpoints.
// bpts array contains nadd bpts to add, followed by ndel bpts to del.
// returns number of successfully modified bpts, -1-network error
// This function is called from debthread
static int idaapi update_bpts(update_bpt_info_t *bpts, int nadd, int ndel)
{
	CHECK_FOR_START(0);

	int i;
	int cnt = 0;

	for (i = 0; i < nadd; ++i)
	{
		switch (bpts[i].type)
		{
		case BPT_EXEC:
			get_debugger()->breakpoint_set(bpts[i].ea);
			bpts[i].code = BPT_OK;
			cnt++;
			break;

		case BPT_READ:
			get_debugger()->watchpoint_set(*get_addr_space(), WATCHPOINT_READ, bpts[i].ea, 1, NULL, NULL);
			bpts[i].code = BPT_OK;
			cnt++;
			break;

		case BPT_WRITE:
			get_debugger()->watchpoint_set(*get_addr_space(), WATCHPOINT_WRITE, bpts[i].ea, 1, NULL, NULL);
			bpts[i].code = BPT_OK;
			cnt++;
			break;

		case BPT_RDWR:
			get_debugger()->watchpoint_set(*get_addr_space(), WATCHPOINT_READWRITE, bpts[i].ea, 1, NULL, NULL);
			bpts[i].code = BPT_OK;
			cnt++;
			break;
		}
	}

	for (i = 0; i < ndel; i++)
	{
		bpts[nadd + i].code = BPT_OK;
		cnt++;

		int idx;
		switch (bpts[i].type)
		{
		case BPT_EXEC:
			if ((idx = get_bpt_index(bpts[i].ea)) >= 0)
				get_debugger()->breakpoint_clear(idx);
			break;

		case BPT_READ:
		case BPT_WRITE:
		case BPT_RDWR:
			if ((idx = get_wpt_index(bpts[i].ea)) >= 0)
				get_debugger()->watchpoint_clear(idx);
			break;
		}
	}

	return cnt;
}

// Update low-level (server side) breakpoint conditions
// Returns nlowcnds. -1-network error
// This function is called from debthread
static int idaapi update_lowcnds(const lowcnd_t *lowcnds, int nlowcnds)
{
	for (int i = 0; i < nlowcnds; ++i)
	{
	}

	return nlowcnds;
}

int main()
{
	return 0;
}

//--------------------------------------------------------------------------
//
//      DEBUGGER DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------

debugger_t debugger =
{
	IDD_INTERFACE_VERSION,
	"MESSIDA", // Short debugger name
	123, // Debugger API module id
	"m68k", // Required processor name
	DBG_FLAG_NOHOST | DBG_FLAG_FAKE_ATTACH | DBG_FLAG_SAFE | DBG_FLAG_NOPASSWORD | DBG_FLAG_NOSTARTDIR | DBG_FLAG_LOWCNDS | DBG_FLAG_CONNSTRING,

	register_classes, // Array of register class names
	RC_GENERAL, // Mask of default printed register classes
	registers, // Array of registers
	qnumber(registers), // Number of registers

	0x1000, // Size of a memory page

	NULL, // bpt_bytes, // Array of bytes for a breakpoint instruction
	NULL, // bpt_size, // Size of this array
	0, // for miniidbs: use this value for the file type after attaching
	0, // reserved

	init_debugger,
	term_debugger,

	process_get_info,

	start_process,
	NULL, // vamos_attach_process,
	NULL, // vamos_detach_process,
	rebase_if_required_to,
	prepare_to_pause_process,
	mess_exit_process,

	get_debug_event,
	continue_after_event,

	NULL, // set_exception_info
	stopped_at_debug_event,

	thread_suspend,
	thread_continue,
	thread_set_step,

	read_registers,
	write_register,

	NULL, // thread_get_sreg_base

	get_memory_info,
	read_memory,
	write_memory,

	is_ok_bpt,
	update_bpts,
	update_lowcnds,

	NULL, // open_file
	NULL, // close_file
	NULL, // read_file

	NULL, // map_address,

	NULL, // set_dbg_options
	NULL, // get_debmod_extensions
	NULL, // update_call_stack

	NULL, // appcall
	NULL, // cleanup_appcall

	NULL, // eval_lowcnd

	NULL, // write_file

	NULL, // send_ioctl
};