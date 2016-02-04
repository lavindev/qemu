#ifndef __VTX_H
#define __VTX_H


/** Reference: 24.2 */

typedef struct QEMU_PACKED segment_registers_state {

	uint16_t selector;
	target_ulong base_addr;
	uint32_t segment_limit;
	struct QEMU_PACKED _access_rights {
		uint32_t segment_type:4;
		uint32_t descriptor_type:1;
		uint32_t dpl:2;
		uint32_t segment_present:1;
		uint32_t reserved0:4;
		uint32_t avl:1;
		uint32_t reserved1:1;
		uint32_t db:1;
		uint32_t granularity:1;
		uint32_t unusable:1;
		uint32_t reserved2:15;
	} access_rights;
} segment_state_t;

typedef struct QEMU_PACKED table_registers_state {
	target_ulong base_addr;
	uint32_t limit;
} table_state_t;

struct QEMU_PACKED vmcs_guest_state_area {

	// REGISTER STATE
	target_ulong cr0;
	target_ulong cr3;
	target_ulong cr4;

	target_ulong dr7;

	target_ulong esp;
	target_ulong eip;
	target_ulong eflags;

	segment_state_t cs;
	segment_state_t ss;
	segment_state_t ds;
	segment_state_t es;
	segment_state_t fs;
	segment_state_t gs;
	segment_state_t ldtr;
	segment_state_t tr;

	table_state_t gdtr;
	table_state_t idtr;

	uint64_t msr_ia32_debugctl;
	uint32_t msr_sysenter_cs;
	target_ulong msr_ia32_sysenter_esp;
	target_ulong msr_ia32_sysenter_eip;
	uint64_t msr_ia32_perf_global_ctrl;
	uint64_t msr_ia32_pat;
	uint64_t msr_ia32_efer;

	uint32_t smbase;

	// NON REGISTER STATE
	uint32_t activity_state;
	#define ACTIVITY_STATE_ACTIVE 		(1U << 0)
	#define ACTIVITY_STATE_HLT 			(1U << 1)
	#define ACTIVITY_STATE_SHUTDOWN 	(1U << 2)
	#define ACTIVITY_STATE_WAITSIPI 	(1U << 3)

	uint32_t interruptibility_state;
	#define INTERRUPTIBILITY_STIBLOCK 	(1U << 0)
	#define INTERRUPTIBILITY_MOVSSBLOCK (1U << 1)
	#define INTERRUPTIBILITY_SMIBLOCK 	(1U << 2)
	#define INTERRUPTIBILITY_NMIBLOCK 	(1U << 3)

	struct QEMU_PACKED _pending_debug_exceptions {
		uint32_t b0:1;
		uint32_t b1:1;
		uint32_t b2:1;
		uint32_t b3:1;
		uint32_t reserved0:8;
		uint32_t enabled_breakpoint:1;
		uint32_t reserved1:1;
		uint32_t bs:1;
		uint64_t reserved2:49;
	} pending_debug_exceptions;

	uint64_t vmcs_link_ptr;
	uint32_t vmx_preemption_timer;
	uint64_t pdpte[4];

	struct QEMU_PACKED _guest_interrupt_status {
		uint8_t rvi;
		uint8_t svi;
	} guest_interrupt_status;
};

struct QEMU_PACKED vmcs_host_state_area {
	target_ulong cr0;
	target_ulong cr3;
	target_ulong cr4;

	target_ulong esp;
	target_ulong eip;

	uint16_t cs_selector;
	uint16_t ss_selector;
	uint16_t ds_selector;
	uint16_t es_selector;
	uint16_t fs_selector;
	uint16_t gs_selector;
	uint16_t tr_selector;

	target_ulong fs_base_addr;
	target_ulong gs_base_addr;
	target_ulong tr_base_addr;
	target_ulong gdtr_base_addr;
	target_ulong idtr_base_addr;

	uint32_t msr_sysenter_cs;
	target_ulong msr_ia32_sysenter_esp;
	target_ulong msr_ia32_sysenter_eip;
	uint64_t msr_ia32_perf_global_ctrl;
	uint64_t msr_ia32_pat;
	uint64_t msr_ia32_efer;
};

struct QEMU_PACKED vmcs_vmexecution_control_fields {

	uint32_t async_event_control;
	#define VM_EXEC_ASYNC_EXTERNAL_INT_EXIT  	(1U << 0)
	#define VM_EXEC_ASYNC_NMI_EXIT				(1U << 3)
	#define VM_EXEC_ASYNC_VIRT_NMI				(1U << 5)
	#define VM_EXEC_ASYNC_ACTIVATE_TIMER		(1U << 6)
	#define VM_EXEC_ASYNC_PROCESS_INT			(1U << 7)


	uint32_t primary_control;	
	#define VM_EXEC_PRIM_INTERRUPT_WINDOW_EXITING 	(1U << 2)
	#define VM_EXEC_PRIM_USE_TSC_OFFSETTING 		(1U << 3)
	#define VM_EXEC_PRIM_HLT_EXITING 				(1U << 7)
	#define VM_EXEC_PRIM_INVLPG_EXITING 			(1U << 9)
	#define VM_EXEC_PRIM_MWAIT_EXITING 				(1U << 10)
	#define VM_EXEC_PRIM_RDPMC_EXITING 				(1U << 11)
	#define VM_EXEC_PRIM_RDTSC_EXITING 				(1U << 12)
	#define VM_EXEC_PRIM_CR3_LOAD_EXITING 			(1U << 15)
	#define VM_EXEC_PRIM_CR3_STORE_EXITING 			(1U << 16)
	#define VM_EXEC_PRIM_CR8_LOAD_EXITING 			(1U << 19)
	#define VM_EXEC_PRIM_CR8_STORE_EXITING 			(1U << 20)
	#define VM_EXEC_PRIM_USE_TPR_SHADOW 			(1U << 21)
	#define VM_EXEC_PRIM_NMI_WINDOW_EXITING 		(1U << 22)
	#define VM_EXEC_PRIM_MOV_DR_EXITING 			(1U << 23)
	#define VM_EXEC_PRIM_UNCONDITIONAL_IO_EXITING 	(1U << 24)
	#define VM_EXEC_PRIM_USE_IO_BITMAPS 			(1U << 25)
	#define VM_EXEC_PRIM_MONITOR_TRAP_FLAG 			(1U << 27)
	#define VM_EXEC_PRIM_USE_MSR_BITMAPS 			(1U << 28)
	#define VM_EXEC_PRIM_MONITOR_EXITING 			(1U << 29)
	#define VM_EXEC_PRIM_PAUSE_EXITING 				(1U << 30)
	#define VM_EXEC_PRIM_ACTIVATE_SECONDARY 		(1U << 31)

	uint32_t secondary_control;
	#define VM_EXEC_SEC_VIRT_APIC_ACCESS 		(1U << 0)
	#define VM_EXEC_SEC_ENABLE_EPT 				(1U << 1)
	#define VM_EXEC_SEC_DESCRIPTOR_TABLE_EXIT 	(1U << 2)
	#define VM_EXEC_SEC_ENABLE_RDTSCP 			(1U << 3)
	#define VM_EXEC_SEC_VIRT_x2APIC 			(1U << 4)
	#define VM_EXEC_SEC_ENABLE_VPID 			(1U << 5)
	#define VM_EXEC_SEC_WBINVD_EXIT 			(1U << 6)
	#define VM_EXEC_SEC_UNRESTRICTED_GUEST 		(1U << 7)
	#define VM_EXEC_SEC_APIC_REG_VIRT 			(1U << 8)
	#define VM_EXEC_SEC_VIRTUAL_INT_DELIVERY 	(1U << 9)
	#define VM_EXEC_SEC_PAUSE_LOOP_EXIT 		(1U << 10)
	#define VM_EXEC_SEC_RDRAND_EXIT 			(1U << 11)
	#define VM_EXEC_SEC_ENABLE_INVPCID 			(1U << 12)
	#define VM_EXEC_SEC_ENABLE_VM_FUNC 			(1U << 13)
	#define VM_EXEC_SEC_VMCS_SHADOWING 			(1U << 14)
	#define VM_EXEC_SEC_RDSEED_EXIT 			(1U << 16)
	#define VM_EXEC_SEC_EPT_VIOLATION_VE 		(1U << 18)
	#define VM_EXEC_SEC_ENABLE_XSAVES_XSRSTORS 	(1U << 20)
	#define VM_EXEC_SEC_TSC_SCALING 			(1U << 25)
	
	uint32_t exception_bitmap;
	/* one bit per exception */

	uint64_t io_bitmap_addr_A;
	uint64_t io_bitmap_addr_B;

	uint64_t tsc_offset;
	uint64_t tsc_multiplier;

	target_ulong mask_cr0;
	target_ulong read_shadow_cr0;
	target_ulong mask_cr4;
	target_ulong read_shadow_cr4;

	target_ulong cr3_target[4];
	uint32_t cr3_target_count;

	uint64_t apic_access_address;
	uint64_t virt_apic_address;
	uint32_t tpr_threshold;
	uint64_t eoi_exit_bitmap[4];
	uint16_t posted_interrupt_notification_vector;
	uint64_t posted_interrupt_descriptor_addr;

	uint64_t read_bitmap_low_msr;
	uint64_t read_bitmap_high_msr;
	uint64_t write_bitmap_low_msr;
	uint64_t write_bitmap_high_msr;

	uint64_t executive_vmcs_pointer;

	struct QEMU_PACKED {
		uint32_t mem_type: 3;
		#define EPTP_MEM_UNCACHEABLE 1
		#define EPTP_MEM_WRITEBACK 6

		uint32_t ept_page_walk_len_decr_1:3;
		uint32_t access_dirty:1;
		uint32_t reserved0:5;
		//#assume physical address is 32 bits
		uint32_t rpt_pml4_table:20;
		uint32_t reserved;
	} eptp;

	uint16_t vpid;
	uint32_t ple_gap;
	uint32_t ple_window;

	uint64_t vm_function_control_vector;
	uint64_t eptp_list_address;

	uint64_t virt_exception_info_addr;
	uint16_t eptp_index;

	uint64_t xss_exiting_bitmap;
};

struct QEMU_PACKED vmcs_vmexit_control_fields {
	
	uint32_t vmexit_controls;
	#define VM_EXIT_SAVE_DEBUG_CONTROLS 	(1U << 2)
	#define VM_EXIT_HOST_ADDR_SPACE_SIZE 	(1U << 9)
	#define VM_EXIT_LD_PERF_GLOB 			(1U << 12)
	#define VM_EXIT_ACK_INT 				(1U << 15)
	#define VM_EXIT_SAVE_PAT 				(1U << 18)
	#define VM_EXIT_LOAD_PAT 				(1U << 19)
	#define VM_EXIT_SAVE_EFER 				(1U << 20)
	#define VM_EXIT_LOAD_EFER 				(1U << 21)
	#define VM_EXIT_SAVE_PREEMPT_TIMER 		(1U << 22)

	uint32_t msr_store_count;
	uint64_t msr_store_addr;
	uint32_t msr_load_count;
	uint64_t msr_load_addr;
};

struct QEMU_PACKED vmcs_vmentry_control_fields {
	uint32_t vmentry_controls;
	#define VM_ENTRY_LOAD_DEBUG_CONTROLS 	(1U << 2)
	#define VM_ENTRY_GUEST					(1U << 9)
	#define VM_ENTRY_SMM_ENTRY				(1U << 10)
	#define VM_ENTRY_DEACTIVATE_DUAL_MON	(1U << 11)
	#define VM_ENTRY_LOAD_PERF_GLOB			(1U << 13)
	#define VM_ENTRY_LOAD_PAT				(1U << 14)
	#define VM_ENTRY_LOAD_EFER				(1U << 15)

	uint32_t msr_load_count;
	uint64_t msr_load_addr;

	struct QEMU_PACKED {
		uint8_t vector;
		uint32_t type:3;
		#define INT_TYPE_EXT 			0x0
		#define INT_TYPE_RESVD 			0x1
		#define INT_TYPE_NMI 			0x2
		#define INT_TYPE_HW 			0x3
		#define INT_TYPE_SW 			0x4
		#define INT_TYPE_PRIV_SW_EXCEP 	0x5
		#define INT_TYPE_SW_EXCEP 		0x6
		#define INT_TYPE_OTHER 			0x7
		uint32_t deliver_err_code:1;
		uint32_t reserved0:19;
		uint32_t valid:1;
	} interruption_info;

	uint32_t exception_err_code;
	uint32_t instruction_length;
};

struct QEMU_PACKED vmcs_vmexit_information_fields {

	struct QEMU_PACKED {
		uint16_t basic_reason;
		uint32_t reserved0:12;
		uint32_t pending_mtf_vm_exit:1;
		uint32_t vm_exit_from_vmx_root_op:1;
		uint32_t reserved1:1;
		uint32_t vm_entry_failure:1;
	} exit_reason;
	target_ulong exit_qualification;
	target_ulong guest_linear_addr;
	uint64_t guest_phys_addr;
	struct QEMU_PACKED {
		uint8_t vector;
		uint32_t type:3;
		uint32_t error_code_valid:1;
		uint32_t nmi_unblocking_iret:1;
		uint32_t reserved0:18;
		uint32_t valid:1;
	} interruption_info;

	uint32_t interruption_error_code;

	struct QEMU_PACKED {
		uint8_t vector;
		uint32_t type:3;
		uint32_t err_code_valid:1;
		uint32_t undefined:1;
		uint32_t reserved0:18;
		uint32_t valid:1;
	} idt_vectoring_info;
	uint32_t idt_vectoring_err_code;

	uint32_t instruction_length;
	uint32_t instruction_info;

	uint32_t instruction_error_field;

};

typedef struct QEMU_PACKED vtx_vmcs {

	uint32_t revision_identifier:31;
	uint32_t shadow_vmcs_indicator:1;
	uint32_t vmx_abort_indicator;

	struct vmcs_guest_state_area vmcs_guest_state_area;
	struct vmcs_host_state_area vmcs_host_state_area;
	struct vmcs_vmexecution_control_fields vmcs_vmexecution_control_fields;
	
	struct vmcs_vmexit_control_fields vmcs_vmexit_control_fields;
	struct vmcs_vmentry_control_fields vmcs_vmentry_control_fields;
	struct vmcs_vmexit_information_fields vmcs_vmexit_information_fields;

	/** Left off at page 16 -- */

	uint32_t launch_state;
	#define LAUNCH_STATE_CLEAR 0
	#define LAUNCH_STATE_LAUNCHED 1

} vtx_vmcs_t;

typedef enum vm_exception {
	SUCCEED,
	FAIL,
	FAIL_INVALID,
	FAIL_VALID
} vm_exception_t;




#endif
