#include "cpu.h"
#include "vtx.h"
#include "exec/cpu-all.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"

#define ISSET(item, flag) (item & flag)
#define or ||
#define and &&

// #define INCLUDE

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
	/* Address range monitoring not implemented in QEMU */
	return;
}


static bool vmlaunch_check_vmx_controls_execution(CPUX86State * env){

	return true;
	
	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	/* 1. TODO: reserved bits Appendinix A.3.1 */
	/* 2. TODO: reserved bits A.3.2 */

	struct vmcs_vmexecution_control_fields * c = &(vmcs->vmcs_vmexecution_control_fields);

	bool check_secondary = true;
	uint32_t primary_control = c->primary_control;
	uint32_t secondary_control = c->secondary_control;
	uint32_t async_event_control = c->async_event_control;

	if (ISSET(primary_control, VM_EXEC_PRIM_ACTIVATE_SECONDARY)){
		/* reserved bits in secondary must be cleared */
	} else {
		check_secondary = false;
	}

	if (c->cr3_target_count > 4) return false;

	if (ISSET(primary_control, VM_EXEC_PRIM_USE_IO_BITMAPS)){
		if ( (c->io_bitmap_addr_A & 0xFFF) || (c->io_bitmap_addr_A >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->io_bitmap_addr_B & 0xFFF) || (c->io_bitmap_addr_B >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
	}

	if (ISSET(primary_control, VM_EXEC_PRIM_USE_MSR_BITMAPS)){
		if ( (c->read_bitmap_low_msr & 0xFFF) || (c->read_bitmap_low_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->read_bitmap_high_msr & 0xFFF) || (c->read_bitmap_high_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->write_bitmap_low_msr & 0xFFF) || (c->write_bitmap_low_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->write_bitmap_high_msr & 0xFFF) || (c->write_bitmap_high_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
	}

	if (ISSET(primary_control, VM_EXEC_PRIM_USE_TPR_SHADOW)){
		if ( (c->virt_apic_address & 0xFFF) || (c->virt_apic_address >> TARGET_PHYS_ADDR_SPACE_BITS) ) return false;
		/* TODO: clear vtpr */
	}

	if (check_secondary && 
		ISSET(primary_control, VM_EXEC_PRIM_USE_TPR_SHADOW) && 
		!ISSET(secondary_control, VM_EXEC_SEC_VIRT_APIC_ACCESS) &&
		!ISSET(secondary_control, VM_EXEC_SEC_VIRTUAL_INT_DELIVERY)){
		/* tpr_threshold[7:4] must not be greater than VTPR */
	}

	if (check_secondary && 
		!ISSET(async_event_control, VM_EXEC_ASYNC_NMI_EXIT) && ISSET(secondary_control, VM_EXEC_SEC_VIRTUAL_INT_DELIVERY)){
		return false;
	}

	if (!ISSET(async_event_control,VM_EXEC_ASYNC_VIRT_NMI) && ISSET(primary_control, VM_EXEC_PRIM_NMI_WINDOW_EXITING)){
		return false;
	}

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_VIRT_APIC_ACCESS) &&
		((c->apic_access_address & 0xFFF) || (c->apic_access_address >> TARGET_PHYS_ADDR_SPACE_BITS))){
		return false;
	}

	if (check_secondary &&
		!ISSET(primary_control, VM_EXEC_PRIM_USE_TPR_SHADOW) &&
		(ISSET(secondary_control, VM_EXEC_SEC_VIRT_x2APIC) || ISSET(secondary_control, VM_EXEC_SEC_APIC_REG_VIRT) || ISSET(secondary_control, VM_EXEC_SEC_VIRTUAL_INT_DELIVERY))){
		return false;
	}

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_VIRT_x2APIC) && ISSET(secondary_control, VM_EXEC_SEC_VIRT_APIC_ACCESS)){
		return false;
	}

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_VIRTUAL_INT_DELIVERY) && !ISSET(async_event_control, VM_EXEC_ASYNC_EXTERNAL_INT_EXIT)){
		return false;
	}

	if (0) return false; /* later */

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_ENABLE_VPID)){
		if (c->vpid == 0x000) return false;
	}

	if (0) return false; /* later */

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_UNRESTRICTED_GUEST) && !ISSET(secondary_control, VM_EXEC_SEC_ENABLE_EPT)){
		return false;
	}

	if (0) return false; /* later */


	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_VMCS_SHADOWING)){
		if ( (c->read_bitmap_low_msr & 0xFFF) || (c->read_bitmap_low_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->read_bitmap_high_msr & 0xFFF) || (c->read_bitmap_high_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->write_bitmap_low_msr & 0xFFF) || (c->write_bitmap_low_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
		if ( (c->write_bitmap_high_msr & 0xFFF) || (c->write_bitmap_high_msr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;
	}

	if (check_secondary &&
		ISSET(secondary_control, VM_EXEC_SEC_EPT_VIOLATION_VE)){
		if ( (c->virt_exception_info_addr & 0xFFF) || (c->virt_exception_info_addr >> TARGET_PHYS_ADDR_SPACE_BITS)) return false;	
	}

	return true;
}

static bool vmlaunch_check_vmx_controls_exit(CPUX86State * env){
	return true;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	struct vmcs_vmexecution_control_fields * c = &(vmcs->vmcs_vmexecution_control_fields);
	struct vmcs_vmexit_control_fields * e = &(vmcs->vmcs_vmexit_control_fields);

	uint32_t vmexit_controls = e->vmexit_controls;

	if (0 /* check reserved bits */){
		return false;
	}

	if (!ISSET(c->async_event_control, VM_EXEC_ASYNC_ACTIVATE_TIMER) && ISSET(vmexit_controls, VM_EXIT_SAVE_PREEMPT_TIMER)){
		return false;
	}

	if (e->msr_store_count){
		if ((e->msr_store_addr & 0xF) ||
			(e->msr_store_addr >> TARGET_PHYS_ADDR_SPACE_BITS)){
			return false;
		}

		if ((uint64_t)(e->msr_store_addr + (e->msr_store_count * 16) - 1) >> TARGET_PHYS_ADDR_SPACE_BITS){
			return false;
		}
	}

	/* IA32_VMX_BASIX[48] -- something */

	if (e->msr_load_count){
		if ((e->msr_load_addr & 0xF) ||
			(e->msr_load_addr >> TARGET_PHYS_ADDR_SPACE_BITS)){
			return false;
		}

		if ((uint64_t)(e->msr_load_addr + (e->msr_load_count * 16) - 1) >> TARGET_PHYS_ADDR_SPACE_BITS){
			return false;
		}
	}

	/* IA32_VMX_BASIX[48] -- something */

	return true;

}

static bool vmlaunch_check_vmx_controls_entry(CPUX86State * env){
	return true;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	//struct vmcs_vmexecution_control_fields * c = &(vmcs->vmcs_vmexecution_control_fields);
	struct vmcs_vmentry_control_fields * e = &(vmcs->vmcs_vmentry_control_fields);

	if (0 /* check reserved bits */){
		return false;
	}

	/* some event injection stuff */


	if (e->msr_load_count){
		if (e->msr_load_addr & 0xFFF || e->msr_load_addr >> TARGET_PHYS_ADDR_SPACE_BITS) 
			return false;
		if ((uint64_t)(e->msr_load_addr + (e->msr_load_count * 16) - 1) >> TARGET_PHYS_ADDR_SPACE_BITS){
			return false;
		}
	}
	/* IA32_VMX_BASIX[48] -- something */

	/* SMM check */

	if (ISSET(e->vmentry_controls, VM_ENTRY_SMM_ENTRY) && ISSET(e->vmentry_controls, VM_ENTRY_DEACTIVATE_DUAL_MON))
		return false;


	return true;
	
}

static bool vmlaunch_check_host_state(CPUX86State * env){
	return true;

	/* a bunch of reserved checks -- not important for now */

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	struct vmcs_host_state_area * h = &(vmcs->vmcs_host_state_area);
	struct vmcs_vmexit_control_fields * ex = &(vmcs->vmcs_vmexit_control_fields);	
	struct vmcs_vmentry_control_fields * en = &(vmcs->vmcs_vmentry_control_fields);	

	if (h->cs_selector & 0x7) return false;
	if (h->ss_selector & 0x7) return false;
	if (h->ds_selector & 0x7) return false;
	if (h->es_selector & 0x7) return false;
	if (h->fs_selector & 0x7) return false;
	if (h->gs_selector & 0x7) return false;
	if (h->tr_selector & 0x7) return false;

	if (h->cs_selector == 0x0000 || h->tr_selector == 0x0000)
		return false;

	if (!ISSET(ex->vmexit_controls, VM_EXIT_HOST_ADDR_SPACE_SIZE) && h->ss_selector == 0x0000)
		return false;

	/* 64 bit stuff in 26.2.3 */

	/* 64 bit stuff in 26.2.4 */

	if (ISSET(en->vmentry_controls, VM_ENTRY_GUEST) || ISSET(ex->vmexit_controls, VM_EXIT_HOST_ADDR_SPACE_SIZE))
		return false;

	return true;

}

/** 26.2 */
static bool vmlaunch_check_vmx_controls(CPUX86State * env){

	return vmlaunch_check_vmx_controls_execution(env) &&
		   vmlaunch_check_vmx_controls_exit(env) &&
		   vmlaunch_check_vmx_controls_entry(env) &&
		   vmlaunch_check_host_state(env);

}

/** 26.3 */
static bool vmlaunch_check_guest_state(CPUX86State * env){
	return true;

}

static void vmlaunch_load_guest_state(CPUX86State * env){

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;
	struct vmcs_guest_state_area * g = &(vmcs->vmcs_guest_state_area);
	struct vmcs_vmentry_control_fields * cf = &(vmcs->vmcs_vmentry_control_fields);

	target_ulong reserved_mask;

	/* Loading Guest Control Registers, Debug Registers, and MSRs */
	{
		/* load CR0 */
		reserved_mask = 0x7FF8FFD0;
		env->cr[0] &= ~reserved_mask;
		env->cr[0] |= (g->cr0 & ~reserved_mask);

		/* load cr3 & 4 */
		env->cr[3] = g->cr3;
		env->cr[4] = g->cr4;

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_DEBUG_CONTROLS)){
				
			/* load dr7 with set bits */
			env->dr[7] = g->dr7;
			env->dr[7] |= 0x400;
			env->dr[7] &= ~(0xD000);

			/* load MSR */
			env->msr_ia32_debugctl = g->msr_ia32_debugctl;
		}

		/* load MSR */
		env->sysenter_cs = g->msr_sysenter_cs;
		env->sysenter_esp = g->msr_ia32_sysenter_esp;
		env->sysenter_eip = g->msr_ia32_sysenter_eip;

		// 64-bit stuff here TODO

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_PERF_GLOB)){
			env->msr_global_ctrl = g->msr_ia32_perf_gloval_ctrl;
		}

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_PAT)){
			env->pat = g->msr_ia32_pat;
		}

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_EFER)){
			env->efer = g->msr_ia32_efer;
		}
	}

	/* Loading Guest Segment Registers and Descriptor-Table Registers */
	{
		// exceptions in 32 bit only
		if (g->cs.access_rights.unusable) raise_exception(env, EXCP0D_GPF);
		if (g->ss.access_rights.unusable) raise_exception(env, EXCP0C_STACK);
		if (g->ds.access_rights.unusable) raise_exception(env, EXCP0D_GPF);
		if (g->es.access_rights.unusable) raise_exception(env, EXCP0D_GPF);
		if (g->fs.access_rights.unusable) raise_exception(env, EXCP0D_GPF);
		if (g->gs.access_rights.unusable) raise_exception(env, EXCP0D_GPF);

		// all modes (64, 32)
		if (g->ldtr.access_rights.unusable) raise_exception(env, EXCP0D_GPF);

		#define LOAD_ACCESS_RIGHTS(flags, access_rights) do {	\
			uint32_t newflags = 0;								\
			newflags |= access_rights.segment_type << 8;		\
			newflags |= access_rights.descriptor_type << 12;	\
			newflags |= access_rights.dpl << DESC_DPL_SHIFT;	\
			newflags |= access_rights.segment_present << 15;	\
			newflags |= access_rights.avl << 20;				\
			newflags |= access_rights.db << DESC_B_SHIFT;		\
			newflags |= access_rights.granularity << 23;		\
			flags = newflags;									\
		} while(0)												
		
		// load tr
		env->tr.selector = g->tr.selector;
		env->tr.base = g->tr.base_addr;
		env->tr.limit = g->tr.segment_limit;
		LOAD_ACCESS_RIGHTS(env->tr.flags, g->tr.access_rights);

		// load cs -- NOTE: change to use function in cpu.h
		env->segs[R_CS].selector = g->cs.selector;
		env->segs[R_CS].base = g->cs.base_addr;
		env->segs[R_CS].limit = g->cs.segment_limit;
		if (g->cs.access_rights.unusable){
			reserved_mask = DESC_G_MASK | DESC_B_MASK | DESC_L_MASK;
			env->segs[R_CS].flags &= ~reserved_mask;
			// NOTE: 64 bit needs L flag below
			env->segs[R_CS].flags |= (g->cs.access_rights.db << DESC_B_SHIFT) | (g->cs.access_rights.granularity << 23);
		} else {
			LOAD_ACCESS_RIGHTS(env->segs[R_CS].flags, g->cs.access_rights);
		}

		// SS, DS, ES, FS, GS, and LDTR
		env->segs[R_SS].selector = g->ss.selector;
		if (g->ss.access_rights.unusable){

			env->segs[R_SS].base &= ~(0x0000000F);
			env->segs[R_SS].flags &= ~(DESC_DPL_MASK);
			env->segs[R_SS].flags |= g->ss.access_rights.dpl << DESC_DPL_SHIFT;
			env->segs[R_SS].flags |= (DESC_B_MASK);
		} else {
			env->segs[R_SS].base = g->ss.base_addr;
			env->segs[R_SS].limit = g->ss.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_SS].flags, g->ss.access_rights);
		}

		env->segs[R_DS].selector = g->ds.selector;
		if (!g->ds.access_rights.unusable){
			env->segs[R_DS].base = g->ds.base_addr;
			env->segs[R_DS].limit = g->ds.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_DS].flags, g->ds.access_rights);
		}

		env->segs[R_ES].selector = g->es.selector;
		if (!g->es.access_rights.unusable){
			env->segs[R_ES].base = g->es.base_addr;
			env->segs[R_ES].limit = g->es.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_ES].flags, g->es.access_rights);
		}

		env->segs[R_FS].selector = g->fs.selector;
		env->segs[R_FS].base = g->fs.base_addr;
		if (!g->fs.access_rights.unusable){
			env->segs[R_FS].limit = g->fs.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_FS].flags, g->fs.access_rights);
			// some 64 bit stuff
		}

		env->segs[R_GS].selector = g->gs.selector;
		env->segs[R_GS].base = g->gs.base_addr;
		if (!g->gs.access_rights.unusable){
			env->segs[R_GS].limit = g->gs.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_GS].flags, g->gs.access_rights);
			// some 64 bit stuff
		}

		env->ldt.selector = g->ldtr.selector;
		if (!g->ldtr.access_rights.unusable){
			env->ldt.base = g->ldtr.base_addr;
			env->ldt.limit = g->ldtr.segment_limit;
			LOAD_ACCESS_RIGHTS(env->ldt.flags, g->ldtr.access_rights);
		}

		// GDT, IDT
		env->gdt.base = g->gdtr.base_addr;
		env->gdt.limit = g->gdtr.limit;

		env->idt.base = g->idtr.base_addr;
		env->idt.limit = g->idtr.limit;
	} 

	/* Loading Guest RIP, RSP, and RFLAGS */
	{
		// watch out for 64 bit
		env->regs[R_ESP] = g->esp;
		env->eip = g->eip;
		env->eflags = g->eflags;
	}

	/* Loading Page-Directory-Pointer-Table Entries */
	{
		/* EPT stuff, not implemented */
	}

	/* Updating Non-Register State */
	{
		/* EPT stuff, not implemented */

		/* virtual interrupts, no idea what to do here */
	}

	clear_address_range_monitoring(env);

}

static void vmlaunch_load_msrs(CPUX86State * env){
	
}


/** helper_vtx_vmxon
 * @arg - env - CPU state env
 * @arg - vmxon_region_ptr - virtual address of physical pointer location
 *		  to vmxon_region. 
 *		  Dereference vmxon_region_ptr to get physical addr of vmxon_region. 
 * 		  Dereference this physical addr to get rev (start of vmxon_region)
 */
void helper_vtx_vmxon(CPUX86State *env, target_ulong vmxon_region_ptr) {

	LOG_ENTRY

	CPUState *cs = CPU(x86_env_get_cpu(env));

	int cpl = env->segs[R_CS].selector & 0x3;
	target_ulong addr;
	int32_t rev;

	/* CHECK: DESC_L_MASK 64 bit only? */
	if (!vmxon_region_ptr || 
		!ISSET(env->cr[0], CR0_PE_MASK) || 
		!ISSET(env->cr[4], CR4_VMXE_MASK) || 
		ISSET(env->eflags, VM_MASK) ||  
		(ISSET(env->efer, MSR_EFER_LMA) && !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			LOG("ILLEGAL OP EXCEPTION")
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_DISABLED){

		if (cpl > 0 ||
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
			if ((vmxon_region_ptr & 0xFFF) || (sizeof(target_ulong)*8 > TARGET_PHYS_ADDR_SPACE_BITS && vmxon_region_ptr >> TARGET_PHYS_ADDR_SPACE_BITS)){
				LOG("Not 4KB aligned, or bits set beyond range")
				vm_exception(FAIL_INVALID, 0, env);
			} else {
				// deref vmxon_region_ptr -> addr
				cpu_memory_rw_debug(cs, vmxon_region_ptr, (uint8_t *)&addr, sizeof(target_ulong), 0);
				rev =  (int32_t) x86_ldl_phys(cs, addr);
				/* TODO: cleanup, use structs for msr */
				if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) ||
					rev < 0 /* rev[31] = 1 */){
					LOG("rev[30:0] != MSR or rev[31] == 1")
					vm_exception(FAIL_INVALID, 0, env);
				} else {
					env->vmcs = (void *) VMCS_CLEAR_ADDRESS;
					env->vmx_operation = VMX_ROOT_OPERATION;
					/* TODO: block INIT signals */
					/* TODO: block A20M */
					//env->a20_mask = -1; // no change -- breaks everything if 1/0
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
	if (env->vmx_operation == VMX_DISABLED ||
		!ISSET(env->cr[0], CR0_PE_MASK) ||
		ISSET(env->eflags, VM_MASK) || 
		(ISSET(env->efer, MSR_EFER_LMA) && !ISSET(env->segs[R_CS].flags, DESC_L_MASK))) {
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

#ifdef INCLUDE
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
	
	LOG_ENTRY

	int cpl = env->segs[R_CS].selector & 0x3;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) env->vmcs;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			LOG("ILLEGAL OPCODE EXCEPTION")
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){

		LOG("VM EXIT")
		/* vm exit */
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION")
		raise_exception(env, EXCP0D_GPF);
	} else if (vmcs == NULL or  (uint64_t)vmcs == VMCS_CLEAR_ADDRESS){

		LOG("INVALID/UNINITIALIZED VMCS")
		vm_exception(FAIL_INVALID, 0, env);
	// } else if (/* events blocked by MOV SS */){
		// vm_exception(FAIL, 26, env);
	} else if (vmcs->launch_state != LAUNCH_STATE_CLEAR){
		LOG("ERROR: VM LAUNCH STATE NOT CLEAR")
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


	LOG_EXIT

}

#ifdef INCLUDE
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
