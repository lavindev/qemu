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
	uint64_t msr_ia32_perf_gloval_ctrl;
	uint64_t msr_ia32_pat;
	uint64_t msr_ia32_efer;

	uint32_t smbase;

	// NON REGISTER STATE
	uint32_t activity state;
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
	uint64_t msr_ia32_perf_gloval_ctrl;
	uint64_t msr_ia32_pat;
	uint64_t msr_ia32_efer;
};

typedef struct QEMU_PACKED vtx_vmcs {

	uint32_t revision_identifier:31;
	uint32_t shadow_vmcs_indicator:1;
	uint32_t vmx_abort_indicator;

	struct vmcs_guess_state_area;
	struct vmcs_host_state_area;
	uint32_t vmcs_vmexecution_control_fields;
	uint32_t vmcs_vmexit_control_fields;
	struct vmcs_vmentry_control_fields;
	struct vmcs_vmexit_information_fields;

	/** Left off at page 16 -- */


} vtx_vmcs_t;

#endif
