#include "cpu.h"
#include "vtx.h"
#include "exec/helper-proto.h"
#include "exec/cpu-all.h"

#define ISSET(item, flag) (item & flag)
#define or ||
#define and &&

#define INCLUDE

#define VMCS_CLEAR_ADDRESS 0xFFFFFFFFFFFFFFFF

// HTONL-- probably use bswap
#define REVERSE_ENDIAN_32(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
		                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
		                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
		                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

#define DEBUG
#if defined (DEBUG)
	#define LOG_ENTRY do { printf("ENTRY: %s\n", __FUNCTION__); } while (0);
	#define LOG_EXIT do { printf("EXIT: %s\n", __FUNCTION__); } while (0);
	#define LOG(x) do { printf("VTX LOG: %s, File: %s, Line: %d\n", x, __FILE__, __LINE__); } while(0);
#else
	#define LOG_ENTRY
	#define LOG_EXIT
	#define LOG(x)
#endif

static void vm_exception(vm_exception_t exception, uint32_t err_number, CPUX86State * env){

	/* TODO */
 	/* define error numbers, see 30.4 */
	// Reference: 30.2 Conventions
	target_ulong update_mask;
	if (exception == SUCCEED){
		update_mask = CC_C | CC_P | CC_A | CC_S | CC_Z | CC_O;
		env->eflags &= ~update_mask;
	} else if (exception == FAIL_INVALID){
		update_mask = CC_P | CC_A | CC_S | CC_Z | CC_O;
		env->eflags &= ~update_mask;
		env->eflags |= CC_C;
	} else if(exception == FAIL_VALID){
		update_mask = CC_C | CC_P | CC_A | CC_S | CC_O;
		env->eflags &= ~update_mask;
		env->eflags |= CC_Z;
		// TODO: Set VM instruction error field to ErrNum
	}

}

static void clear_address_range_monitoring(CPUX86State * env){
	return;
}


/** SECTION 23.7 **/
void helper_vtx_vmxon(CPUX86State *env, uint64_t vmxon_region_addr_phys) {

	LOG_ENTRY

	int cpl = env->segs[R_CS].selector & 0x3;
	uint32_t rev_buf=0;
	int32_t rev;

	/* CHECK: DESC_L_MASK 64 bit only? */
	if (!vmxon_region_addr_phys or 
		!ISSET(env->cr[0], CR0_PE_MASK) or 
		!ISSET(env->cr[4], CR4_VMXE_MASK) or 
		ISSET(env->eflags, VM_MASK) or  
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			LOG("ILLEGAL OP EXCEPTION")
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_DISABLED){
		

		if (cpl > 0 or
			/* cr0,4 values not supported */
			!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_LOCK)
			/* smx */
			/* non smx */){
			printf("CPL = %d\n", cpl);
			printf("MSR = %ld\n", env->msr_ia32_feature_control);
			LOG("GENERAL PROTECTION EXCEPTION")
			raise_exception(env, EXCP0D_GPF);
		} else {
			/* check 4KB align or bits beyond phys range*/
			if ((vmxon_region_addr_phys & 0xFFF) or (vmxon_region_addr_phys >> TARGET_PHYS_ADDR_SPACE_BITS)){
				LOG("Not 4KB aligned, or bits set beyond range")
				vm_exception(FAIL_INVALID, 0, env);
			} else {
				// deref vmxon_region_addr_phys -> buf
				//cpu_physical_memory_rw(vmxon_region_addr_phys, (uint8_t *) &rev_buf, 4, 0);
				rev =  (int32_t) REVERSE_ENDIAN_32(rev_buf); // MARK
				/* TODO: cleanup, use structs for msr */
				if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) or
					rev < 0 /* rev[31] = 1 */){
					LOG("rev[30:0] != MSR or rev[31] == 1")
					vm_exception(FAIL_INVALID, 0, env);
				} else {
					env->vmcs = (void *) VMCS_CLEAR_ADDRESS;
					env->vmx_operation = VMX_ROOT_OPERATION;
					/* TODO: block INIT signals */
					/* TODO: block A20M */
					env->a20_mask = -1; // no change -- breaks everything if 1/0
					clear_address_range_monitoring(env);
					vm_exception(SUCCEED, 0, env);
					LOG("<1> In VMX Root Operation")
				}
			}

		}

	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* VM exit */
		LOG("VM EXIT")
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION")
		raise_exception(env, EXCP0D_GPF);
	} else {
		LOG("FAIL");
		vm_exception(FAIL, 15, env);
	}

	LOG_EXIT
}

void helper_vtx_vmxoff(CPUX86State * env){

	LOG_ENTRY

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))) {
			LOG("INVALID OPCODE EXCEPTION")
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
		LOG("VM EXIT")
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION")
		raise_exception(env, EXCP0D_GPF);
	// } else if (/* dual monitor - wtf is this? */){
		// vm_exception(FAIL, 23); // TO, envDO
	} else {
		LOG("DISABLING VMX")
		env->vmx_operation = VMX_DISABLED;
		/* unblock init */
		/* if ia32_smm_monitor[2] == 0, unblock SMIs */
		/* if outside SMX operration, unblock and enable A20M */
		env->a20_mask = -1; // no change -- breaks everything if 1/0		
		clear_address_range_monitoring(env);
		vm_exception(SUCCEED,0, env);
	}

	LOG_EXIT
}

#ifndef INCLUDE
void helper_vtx_vmptrld(CPUX86State * env, uint64_t vmcs_addr_phys){

	int cpl = env->segs[R_CS].selector & 0x3;
	int32_t rev;

	if (env->vmx_operation == VMX_DISABLED or 
		!ISSET(env->cr[0], CR0_PE_MASK) or 
		ISSET(env->eflags, VM_MASK) or  
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vmexit */
	} else if (cpl > 0) {
		raise_exception(env, EXCP0D_GPF);
	} else {
		if ((vmcs_addr_phys & 0xFFF) or (vmcs_addr_phys >> 32))
			vm_exception(FAIL, 9); // to, envdo
		else if (vmcs_addr_phys == (uint64_t) env->vmcs)
			vm_exception(FAIL, 10); // to, envdo
		else {
			rev = *(vmcs_addr_phys) & 0xFFFFFFFF;
			if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) or
				(rev < 0 and /* processor does not support 1 setting of VMCS shadowing*/)
				vm_exception(FAIL, 11); // to, envdo
			else {
				env->vmcs = (void *) vmcs_addr_phys;
				vm_exception(SUCCEED,0, env);
			}
		}
	}

}

void helper_vtx_invept(CPUX86State * env, uint32_t reg, uint64_t mem[2]){

}

void helper_vtx_invvpid(CPUX86State * env, uint32_t reg, uint64_t mem[2]){

}

void helper_vtx_vmcall(CPUX86State * env){

	int cpl = env->segs[R_CS].selector & 0x3;

	if (env->vmx_operation == VMX_DISABLED){
		raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (ISSET(env->eflags, VM_MASK) or (ISSET(env->efer,MSR_EFER_LMA) and  !ISSET(env->segs[R_CS].flags, DESC_L_MASK)) ){
		raise_exception(env, EXCP06_ILLOP);
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (SMM or no dual treatment or !ISSET(env, /* IA32_SMM_MONITOR_CTL */)){
		vm_exception(FAIL, 1, env);
	} else if (/*dual treatment of of SMIs and SMM is active*/){
		/* VMM VM exit? */
	} else if (env->vmcs == NULL or env->vmcs == VMCS_CLEAR_ADDRESS){
		vm_exception(FAIL_INVALID, 0, env);
	} else if (env->vmcs->launch_state != LAUNCH_STATE_CLEAR){
		vm_exception(FAIL, 1, env9)
	} else if (/* vm exit control fields invalid */){
		vm_exception(FAIL, 20, env);
	} else {

		/* enter SMM */
		int rev = /* revision identfier in MSEG */
		if (rev is unsupported){
			/* leave SMM */
			vm_exception(FAIL, 22, env);
		} else {
			int smm_features = /* from MSEG */
			if (smm_features is invalid){
				/*leave SMM */
				vm_exception(FAIL, 24, env);
			} else {
				/* activate dual monitor treatment of SMIs and SMM */
			}
		}

	}

}

void helper_vtx_vmclear(CPUX86State * env, uint64_t vmcs_addr_phys){

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else {
		if ((vmcs_addr_phys & 0xFFF) or (vmcs_addr_phys >> 32)){
			vm_exception(FAIL, 2, env);
		} else if (vmcs_addr_phys == VMCS_CLEAR_ADDRESS){
			vm_exception(FAIL, 3, env);
		} else{
			/* ensure that data for VMCS references by operand is in memory */
			/* initialize V<CS region */
			env->vmcs->launch_state = LAUNCH_STATE_CLEAR;
			if ((uint64_t)(env->vmcs) == vmcs_addr_phys){
				env->vmcs = VMCS_CLEAR_ADDRESS;
			}
			vm_exception(SUCCEED, 0, env);
		}
	}

}

void helper_vtx_vmfunc(CPUX86State * env){

	target_ulong eax = env->regs[R_EAX];

	/* only used for EPTP switching, not implementing for now */

}
#endif

void helper_vtx_vmlaunch(CPUX86State * env){
		
	int cpl = env->segs[R_CS].selector & 0x3;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (vmcs == NULL or  (uint64_t)vmcs == VMCS_CLEAR_ADDRESS){
		vm_exception(FAIL_INVALID, 0, env);
	// } else if (/* events blocked by MOV SS */){
		// vm_exception(FAIL, 26, env);
	} else if (vmcs->launch_state != LAUNCH_STATE_CLEAR){
		vm_exception(FAIL, 4, env);
	} else {
		// /* check settings */
		// if (invalid settings){
		// 	/* approppriate vm_exception , env*/
		// } else {
		// 	/* attempt to load guest state and PDPTRs as appropraite */
		// 	/* clear address range monitoring */
		// 	if (fail){
		// 		vm entry fails
		// 	} else {
		// 		/* attempt to load MSRs*/
		// 		if (fail_msr){
		// 			vm entry fails
		// 		} else {
		// 			if (VMLAUNCH){
		// 				vmcs->launch_state = LAUNCH_STATE_LAUNCHED;
		// 			}
		// 			if (SMM && entry to SMM control is 0){
		// 				if ( deactivate dual mintor treatement == 0){
		// 					smm transfer vmcs = vmcs;
		// 				}
		// 				if (exec vmcs ptr == VMCS_CLEAR_ADDRESS){
		// 					vmcs = vmcs link ptr  //MARK
		// 				} else {
		// 					vmcs = exec vmcs ptr; //MARK
		// 				}
		// 				leave SMM
		// 			}
		// 			vm_exception(SUCCEED, 0, env);
		// 		}
		// 	}
		// }
	}

}
#ifndef INCLUDE

void helper_vtx_vmresume(CPUX86State * env){
		
	int cpl = env->segs[R_CS].selector & 0x3;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if (vmcs == NULL or  (uint64_t)vmcs == VMCS_CLEAR_ADDRESS){
		vm_exception(FAIL_INVALID, 0, env);
	// } else if (/* events blocked by MOV SS */){
		// vm_exception(FAIL, 26, env);
	} else if (VMRESUME && vmcs->launch_state != LAUNCH_STATE_LAUNCHED){
		vm_exception(FAIL, 5, env);
	} else {
		// /* check settings */
		// if (invalid settings){
		// 	/* approppriate vm_exception , env*/
		// } else {
		// 	/* attempt to load guest state and PDPTRs as appropraite */
		// 	/* clear address range monitoring */
		// 	if (fail){
		// 		vm entry fails
		// 	} else {
		// 		/* attempt to load MSRs*/
		// 		if (fail_msr){
		// 			vm entry fails
		// 		} else {
		// 			if (VMLAUNCH){
		// 				vmcs->launch_state = LAUNCH_STATE_LAUNCHED;
		// 			}
		// 			if (SMM && entry to SMM control is 0){
		// 				if ( deactivate dual mintor treatement == 0){
		// 					smm transfer vmcs = vmcs;
		// 				}
		// 				if (exec vmcs ptr == VMCS_CLEAR_ADDRESS){
		// 					vmcs = vmcs link ptr  //MARK
		// 				} else {
		// 					vmcs = exec vmcs ptr; //MARK
		// 				}
		// 				leave SMM
		// 			}
		// 			vm_exception(SUCCEED, 0, env);
		// 		}
		// 	}
		// }
	}

}

void helper_vtx_vmptrst(CPUX86State * env, uint64_t * dest){

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else {
		*dest = (uint64_t) env->vmcs;
		vm_exception(SUCCEED,0, env);
	}

}

void helper_vtx_vmread(CPUX86State * env, target_ulong op1, target_ulong op2){

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION AND /* shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs == VMCS_CLEAR_ADDRESS) or
			   (env->vmx_operation == VMX_NON_ROOT_OPERATION and /* vmcs link ptr */)){
		vm_exception(FAIL_INVALID, 0, env);
	} else if (src op no correpsonse){
		vm_exception(FAIL, 12, env);
	} else {
		if (env->vmx_operation == VMX_ROOT_OPERATION){
			*dest = 
		} else {
			*dest = 
		}
		vm_exception(SUCCEED,0, env);
	}
}

void helper_vtx_vmwrite(CPUX86State * env, target_ulong op1, target_ulong op2){

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION AND /* shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs == VMCS_CLEAR_ADDRESS) or
			   (env->vmx_operation == VMX_NON_ROOT_OPERATION and /* vmcs link ptr */)){
		vm_exception(FAIL_INVALID, 0, env);
	} else if (src op no correpsonse){
		vm_exception(FAIL, 12, env);
	} else {
		if (env->vmx_operation == VMX_ROOT_OPERATION){
			*dest = 
		} else {
			*dest = 
		}
		vm_exception(SUCCEED,0, env);
	}

}
#endif
