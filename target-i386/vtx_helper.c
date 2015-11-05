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

void helper_vtx_invept(CPUX86State * env, uint32_t reg, uint64_t mem[2]){

}

void helper_vtx_invvpid(CPUX86State * env, uint32_t reg, uint64_t mem[2]){

}

void helper_vtx_vmcall(CPUX86State * env){

	int cpl = env[R_CS].selector & 0x3;

	if (env->vmx_operation == VMX_DISABLED){
		raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (ISSET(env->eflags, VM_MASK) or (ISSET(env->efer,MSR_EFER_LMA) and  !ISSET(env->segs[R_CS].flags, DESC_L_MASK)) ){
		raise_exception(env, EXCP06_ILLOP);
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (SMM or no dual treatment or !ISSET(env, /* IA32_SMM_MONITOR_CTL */)){
		vm_exception(FAIL, 1);
	} else if (/*dual treatment of of SMIs and SMM is active*/){
		/* VMM VM exit? */
	} else if (env->vmcs == NULL or env->vmcs == 0xFFFFFFFFFFFFFFFF){
		vm_exception(FAIL_INVALID, 0);
	} else if (env->vmcs->launch_state != LAUNCH_STATE_CLEAR){
		vm_exception(FAIL, 19)
	} else if (/* vm exit control fields invalid */){
		vm_exception(FAIL, 20);
	} else {

		/* enter SMM */
		int rev = /* revision identfier in MSEG */
		if (rev is unsupported){
			/* leave SMM */
			vm_exception(FAIL, 22);
		} else {
			int smm_features = /* from MSEG */
			if (smm_features is invalid){
				/*leave SMM */
				vm_exception(FAIL, 24);
			} else {
				/* activate dual monitor treatment of SMIs and SMM */
			}
		}

	}

}

void helper_vtx_vmclear(CPUX86State * env, uint64_t vmcs_addr_phys){

	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else {
		if ((vmcs_addr_phys & 0xFFF) or (vmcs_addr_phys >> TARGET_LONG_BITS)){
			vm_exception(FAIL, 2);
		} else if (vmcs_addr_phys == 0xFFFFFFFFFFFFFFFF){
			vm_exception(FAIL, 3);
		} else{
			/* ensure that data for VMCS references by operand is in memory */
			/* initialize V<CS region */
			env->vmcs->launch_state = LAUNCH_STATE_CLEAR;
			if ((uint64_t)(env->vmcs) == vmcs_addr_phys){
				env->vmcs = 0xFFFFFFFFFFFFFFFF;
			}
			vm_exception(SUCCEED, 0);
		}
	}

}

void helper_vtx_vmfunc(CPUX86State * env){

	target_ulong eax = env->regs[R_EAX];

	/* only used for EPTP switching, not implementing for now */

}

void helper_vtx_vmlaunch(CPUX86State * env){ /* also vm resume */
		
	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (env->vmcs == NULL or env->vmcs == 0xFFFFFFFFFFFFFFFF){
		vm_exception(FAIL_INVALID, 0);
	} else if (/* events blocked by MOV SS */){
		vm_exception(FAIL, 26);
	} else if (VMLAUNCH && env->vmcs->launch_state != LAUNCH_STATE_CLEAR){
		vm_exception(FAIL, 4);
	} else if (VMRESUME && env->vmcs->launch_state != LAUNCH_STATE_LAUNCHED){
		vm_exception(FAIL, 5)
	} else {
		/* check settings */
		if (invalid settings){
			/* approppriate vm_exception */
		} else {
			/* attempt to load guest state and PDPTRs as appropraite */
			/* clear address range monitoring */
			if (fail){
				vm entry fails
			} else {
				/* attempt to load MSRs*/
				if (fail_msr){
					vm entry fails
				} else {
					if (VMLAUNCH){
						env->vmcs->launch_state = LAUNCH_STATE_LAUNCHED;
					}
					if (SMM && entry to SMM control is 0){
						if ( deactivate dual mintor treatement == 0){
							smm transfer vmcs = env->vmcs;
						}
						if (exec vmcs ptr == 0xFFFFFFFFFFFFFFFF){
							env->vmcs = vmcs link ptr
						} else {
							env->vmcs = exec vmcs ptr;
						}
						leave SMM
					}
					vm_exception(SUCCEED, 0);
				}
			}
		}
	}

}

void helper_vtx_vmptrst(CPUX86State * env, uint64_t * dest){

	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else {
		*dest = (uint64_t) env->vmcs;
		vm_exception(SUCCEED,0);
	}

}

void helper_vtx_vmread(CPUX86State * env, target_ulong op1, target_ulong op2){

	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION AND /* shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs == 0xFFFFFFFFFFFFFFFF) or
			   (env->vmx_operation == VMX_NON_ROOT_OPERATION and /* vmcs link ptr */)){
		vm_exception(FAIL_INVALID, 0);
	} else if (src op no correpsonse){
		vm_exception(FAIL, 12);
	} else {
		if (env->vmx_operation == VMX_ROOT_OPERATION){
			*dest = 
		} else {
			*dest = 
		}
		vm_exception(SUCCEED,0);
	}
}

void helper_vtx_vmwrite(CPUX86State * env, target_ulong op1, target_ulong op2){

	int cpl = env[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK)){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION AND /* shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs == 0xFFFFFFFFFFFFFFFF) or
			   (env->vmx_operation == VMX_NON_ROOT_OPERATION and /* vmcs link ptr */)){
		vm_exception(FAIL_INVALID, 0);
	} else if (src op no correpsonse){
		vm_exception(FAIL, 12);
	} else {
		if (env->vmx_operation == VMX_ROOT_OPERATION){
			*dest = 
		} else {
			*dest = 
		}
		vm_exception(SUCCEED,0);
	}

}

