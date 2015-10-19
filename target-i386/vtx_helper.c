#include "cpu.h"
#include "vtx.h"

#define ISSET(item, flag) (item & flag)

#define DEBUG
#if defined (DEBUG)
#define LOG_ENTRY do { printf("ENTRY: %s\n", __function__); } while (0);
#define LOG(x) do { printf("VTX LOG: %s\n", x); } while(0);
#else
#define LOG_ENTRY
#define LOG(x)
#endif


/** SECTION 23.7 **/
void helper_vtx_vmxon(CPUX86State *env) {

	LOG_ENTRY

	if (!ISSET(env->cr[4], CR4_VMXE_MASK)) {
		// invalid opcode exception
	}

	if (!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_LOCK)) {
		// general protection exception
	}

	if (!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_SMX) && /*SMX OPERATION*/) {
		// general protection exception
	}

	if (!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_NONSMX) && /*NON SMX OPERATION*/) {
		// general protection exception
	}

	// ADD: 23.8 restrictions
}


