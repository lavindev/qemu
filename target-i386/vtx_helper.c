#include "cpu.h"
#include "vtx.h"

#define ISSET(item, flag) (item & flag)
#define or ||
#define and &&


#define DEBUG
#if defined (DEBUG)
	#define LOG_ENTRY do { printf("ENTRY: %s\n", __FUNCTION__); } while (0);
	#define LOG(x) do { printf("VTX LOG: %s\n", x); } while(0);
#else
	#define LOG_ENTRY
	#define LOG(x)
#endif

static void vm_exception(vm_exception_t exception, uint32_t err_number){

	/* TODO */
 	/* define error numbers, see 30.4 */
}


/** SECTION 23.7 **/
void helper_vtx_vmxon(CPUX86State *env, uint64_t vmcs_addr_phys) {

	LOG_ENTRY

	int cpl = env[R_CS].selector & 0x3;

	/* CHECK: DESC_L_MASK 64 bit only? */
	if (!vmcs_addr_phys or 
		!ISSET(env->cr[0], CR0_PE_MASK) or 
		!ISSET(enc->cr[4], CR4_VMXE_MASK) or 
		ISSET(env->eflags, VM_MASK) or  
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_DISABLED){
		
		int32_t rev;

		if (cpl > 0 or
			/* cr0,4 values not supported */
			!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_LOCK)
			/* smx */
			/* non smx */){
			raise_exception(env, EXCP0D_GPF);
		} else {
			/* check 4KB align or bits beyond phys range*/
			if ((vmcs_addr_phys & 0xFFF) or (vmcs_addr_phys >> TARGET_LONG_BITS)){
				vm_exception(FAIL_INVALID, 0);
			} else {
				rev = *(vmcs_addr_phys) & 0xFFFFFFFF;
				/* TODO: cleanup, use structs for msr */
				if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) or
					rev < 0 /* rev[31] = 1 */){
					vm_exception(FAIL_INVALID, 0);
				} else {
					env->vmcs = (void *) 0xFFFFFFFFFFFFFFFF; /* Invalid Address? -1 */
					env->vmx_operation = VMX_ROOT_OPERATION;
					/* TODO: block INIT signals */
					/* TODO: block A20M */
					env->a20_mask = 0;
					/* TODO: clear address range monitoring */
					vm_exception(SUCCEED, 0);
				}
			}

		}

	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* VM exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else {
		vm_exception(FAIL, 15); // TODO: defines
	}
}

void helper_vtx_vmxoff(CPUX86State * env){

	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation != VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (/* dual monitor - wtf is this? */){
		vm_exception(FAIL, 23); // TODO
	} else {
		env->vmx_operation = VMX_DISABLED;
		/* unblock init */
		/* if ia32_smm_monitor[2] == 0, unblock SMIs */
		/* if outside SMX operration, unblock and enable A20M */
		/* clear address range monitoring */
		vm_exception(SUCCEED,0);
	}
}

void helper_vtx_vmptrld(CPUX86State * env, uint64_t vmcs_addr_phys){

	int cpl = env[R_CS].selector & 0x3;
	int32_t rev;

	if (env->vmx_operation == VMX_DISABLED or 
		!ISSET(env->cr[0], CR0_PE_MASK) or 
		ISSET(env->eflags, VM_MASK) or  
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vmexit */
	} else if (cpl > 0) {
		raise_exception(env, EXCP0D_GPF);
	} else {
		if ((vmcs_addr_phys & 0xFFF) or (vmcs_addr_phys >> TARGET_LONG_BITS))
			vm_exception(FAIL, 9); // todo
		else if (vmcs_addr_phys == (uint64_t) env->vmcs)
			vm_exception(FAIL, 10); // todo
		else {
			rev = *(vmcs_addr_phys) & 0xFFFFFFFF;
			if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) or
				(rev < 0 and /* processor does not support 1 setting of VMCS shadowing*/)
				vm_exception(FAIL, 11); // todo
			else {
				env->vmcs = (void *) vmcs_addr_phys;
				vm_exception(SUCCEED,0);
			}
		}
	}

}


