#include "cpu.h"
#include "vtx.h"
#include "exec/cpu-all.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"

#define ISSET(item, flag) ((item) & (flag))
#define or ||
#define and &&

// #define INCLUDE

#define VMCS_CLEAR_ADDRESS 0xFFFFFFFF

#define DEBUG
#if defined (DEBUG)
	#define LOG_ENTRY do { printf("ENTRY: %s\n", __FUNCTION__); } while (0)
	#define LOG_EXIT do {  printf("EXIT: %s\n", __FUNCTION__);  } while (0)
	#define LOG(x) do { printf("VTX LOG: %s, File: %s, Line: %d\n", x, __FILE__, __LINE__); } while(0)
	#define PRINT(x) do { printf("%s\n", #x); } while(0)
#else
	#define LOG_ENTRY
	#define LOG_EXIT
	#define LOG(x)
#endif


static void vtx_vmexit(CPUX86State * env, struct vmcs_vmexit_information_fields * fields, target_ulong eip);

// copies vmcs from guest physicall addr (arg below) to processor vmcs
// need to tediously copy each field due to endianness
// static void load_vmcs_proc(hwaddr addr){

// 	// later -- use in memory vmcs for now

// }

static void flush_field16(CPUX86State * env, uint32_t offset, uint16_t val){

	CPUState *cs = CPU(x86_env_get_cpu(env));
	x86_stw_phys(cs, env->vmcs_ptr_register + offset, val);

}

static void flush_field32(CPUX86State * env, uint32_t offset, uint32_t val){

	CPUState *cs = CPU(x86_env_get_cpu(env));
	x86_stl_phys(cs, env->vmcs_ptr_register + offset, val);

}

static void flush_field64(CPUX86State * env, uint32_t offset, uint64_t val){

	CPUState *cs = CPU(x86_env_get_cpu(env));
	x86_stq_phys(cs, env->vmcs_ptr_register + offset, val);

}
static void flush_field_tl(CPUX86State * env, uint32_t offset, target_ulong val){

	CPUState *cs = CPU(x86_env_get_cpu(env));
	x86_stl_phys(cs, env->vmcs_ptr_register + offset, val);

}


static void flush_active_vmcs(CPUX86State * env){

	if (env->processor_vmcs == NULL)
		return;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vpid), (uint16_t) (vmcs->vmcs_vmexecution_control_fields.vpid));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.posted_interrupt_notification_vector), (uint16_t) (vmcs->vmcs_vmexecution_control_fields.posted_interrupt_notification_vector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp_index), (uint16_t) (vmcs->vmcs_vmexecution_control_fields.eptp_index));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.selector), (uint16_t) (vmcs->vmcs_guest_state_area.es.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.selector), (uint16_t) (vmcs->vmcs_guest_state_area.cs.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.selector), (uint16_t) (vmcs->vmcs_guest_state_area.ss.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.selector), (uint16_t) (vmcs->vmcs_guest_state_area.ds.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.selector), (uint16_t) (vmcs->vmcs_guest_state_area.fs.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.selector), (uint16_t) (vmcs->vmcs_guest_state_area.gs.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.selector), (uint16_t) (vmcs->vmcs_guest_state_area.ldtr.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.selector), (uint16_t) (vmcs->vmcs_guest_state_area.tr.selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.guest_interrupt_status), (uint16_t) (vmcs->vmcs_guest_state_area.guest_interrupt_status_val));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.es_selector), (uint16_t) (vmcs->vmcs_host_state_area.es_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.cs_selector), (uint16_t) (vmcs->vmcs_host_state_area.cs_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.ss_selector), (uint16_t) (vmcs->vmcs_host_state_area.ss_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.ds_selector), (uint16_t) (vmcs->vmcs_host_state_area.ds_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.fs_selector), (uint16_t) (vmcs->vmcs_host_state_area.fs_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.gs_selector), (uint16_t) (vmcs->vmcs_host_state_area.gs_selector));
	flush_field16(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.tr_selector), (uint16_t) (vmcs->vmcs_host_state_area.tr_selector));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.io_bitmap_addr_A), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.io_bitmap_addr_A));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.io_bitmap_addr_B), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.io_bitmap_addr_B));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_bitmap_low_msr), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.read_bitmap_low_msr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_store_addr), (uint64_t) (vmcs->vmcs_vmexit_control_fields.msr_store_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_load_addr), (uint64_t) (vmcs->vmcs_vmexit_control_fields.msr_load_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.msr_load_addr), (uint64_t) (vmcs->vmcs_vmentry_control_fields.msr_load_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.executive_vmcs_pointer), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.executive_vmcs_pointer));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.tsc_offset), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.tsc_offset));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.virt_apic_address), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.virt_apic_address));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.apic_access_address), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.apic_access_address));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.posted_interrupt_descriptor_addr), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.posted_interrupt_descriptor_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vm_function_control_vector), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.vm_function_control_vector));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eptp_val));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[0]), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eoi_exit_bitmap[0]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[1]), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eoi_exit_bitmap[1]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[2]), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eoi_exit_bitmap[2]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[3]), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eoi_exit_bitmap[3]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp_list_address), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.eptp_list_address));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vmread_bitmap), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.vmread_bitmap));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vmwrite_bitmap), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.vmwrite_bitmap));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.virt_exception_info_addr), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.virt_exception_info_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.xss_exiting_bitmap), (uint64_t) (vmcs->vmcs_vmexecution_control_fields.xss_exiting_bitmap));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.guest_phys_addr), (uint64_t) (vmcs->vmcs_vmexit_information_fields.guest_phys_addr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.vmcs_link_ptr), (uint64_t) (vmcs->vmcs_guest_state_area.vmcs_link_ptr));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_debugctl), (uint64_t) (vmcs->vmcs_guest_state_area.msr_ia32_debugctl));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_pat), (uint64_t) (vmcs->vmcs_guest_state_area.msr_ia32_pat));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_efer), (uint64_t) (vmcs->vmcs_guest_state_area.msr_ia32_efer));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_perf_global_ctrl), (uint64_t) (vmcs->vmcs_guest_state_area.msr_ia32_perf_global_ctrl));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[0]), (uint64_t) (vmcs->vmcs_guest_state_area.pdpte[0]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[1]), (uint64_t) (vmcs->vmcs_guest_state_area.pdpte[1]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[2]), (uint64_t) (vmcs->vmcs_guest_state_area.pdpte[2]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[3]), (uint64_t) (vmcs->vmcs_guest_state_area.pdpte[3]));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_pat), (uint64_t) (vmcs->vmcs_host_state_area.msr_ia32_pat));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_efer), (uint64_t) (vmcs->vmcs_host_state_area.msr_ia32_efer));
	flush_field64(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_perf_global_ctrl), (uint64_t) (vmcs->vmcs_host_state_area.msr_ia32_perf_global_ctrl));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.async_event_control), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.async_event_control));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.primary_control), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.primary_control));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.exception_bitmap), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.exception_bitmap));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.page_fault_error_code_mask), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.page_fault_error_code_mask));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.page_fault_error_code_match), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.page_fault_error_code_match));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target_count), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.cr3_target_count));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.vmexit_controls), (uint32_t) (vmcs->vmcs_vmexit_control_fields.vmexit_controls));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_store_count), (uint32_t) (vmcs->vmcs_vmexit_control_fields.msr_store_count));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_load_count), (uint32_t) (vmcs->vmcs_vmexit_control_fields.msr_load_count));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.vmentry_controls), (uint32_t) (vmcs->vmcs_vmentry_control_fields.vmentry_controls));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.msr_load_count), (uint32_t) (vmcs->vmcs_vmentry_control_fields.msr_load_count));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.interruption_info), (uint32_t) (vmcs->vmcs_vmentry_control_fields.interruption_info_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.exception_err_code), (uint32_t) (vmcs->vmcs_vmentry_control_fields.exception_err_code));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.instruction_length), (uint32_t) (vmcs->vmcs_vmentry_control_fields.instruction_length));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.tpr_threshold), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.tpr_threshold));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.secondary_control), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.secondary_control));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.ple_gap), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.ple_gap));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.ple_window), (uint32_t) (vmcs->vmcs_vmexecution_control_fields.ple_window));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_error_field), (uint32_t) (vmcs->vmcs_vmexit_information_fields.instruction_error_field));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.exit_reason), (uint32_t) (vmcs->vmcs_vmexit_information_fields.exit_reason_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.interruption_info), (uint32_t) (vmcs->vmcs_vmexit_information_fields.interruption_info_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.interruption_error_code), (uint32_t) (vmcs->vmcs_vmexit_information_fields.interruption_error_code));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.idt_vectoring_info), (uint32_t) (vmcs->vmcs_vmexit_information_fields.idt_vectoring_info_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.idt_vectoring_err_code), (uint32_t) (vmcs->vmcs_vmexit_information_fields.idt_vectoring_err_code));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_length), (uint32_t) (vmcs->vmcs_vmexit_information_fields.instruction_length));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_info), (uint32_t) (vmcs->vmcs_vmexit_information_fields.instruction_info));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.es.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.cs.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.ss.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.ds.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.fs.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.gs.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.ldtr.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.segment_limit), (uint32_t) (vmcs->vmcs_guest_state_area.tr.segment_limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gdtr.limit), (uint32_t) (vmcs->vmcs_guest_state_area.gdtr.limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.idtr.limit), (uint32_t) (vmcs->vmcs_guest_state_area.idtr.limit));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.es.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.cs.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.ss.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.ds.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.fs.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.gs.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.ldtr.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.access_rights), (uint32_t) (vmcs->vmcs_guest_state_area.tr.access_rights_val));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.interruptibility_state), (uint32_t) (vmcs->vmcs_guest_state_area.interruptibility_state));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.activity_state), (uint32_t) (vmcs->vmcs_guest_state_area.activity_state));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.smbase), (uint32_t) (vmcs->vmcs_guest_state_area.smbase));
	flush_field32(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_sysenter_cs), (uint32_t) (vmcs->vmcs_guest_state_area.msr_sysenter_cs));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.vmx_preemption_timer), (target_ulong) (vmcs->vmcs_guest_state_area.vmx_preemption_timer));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_sysenter_cs), (target_ulong) (vmcs->vmcs_host_state_area.msr_sysenter_cs));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.mask_cr0), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.mask_cr0));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.mask_cr4), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.mask_cr4));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_shadow_cr0), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.read_shadow_cr0));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_shadow_cr4), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.read_shadow_cr4));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[0]), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.cr3_target[0]));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[1]), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.cr3_target[1]));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[2]), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.cr3_target[2]));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[3]), (target_ulong) (vmcs->vmcs_vmexecution_control_fields.cr3_target[3]));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.exit_qualification), (target_ulong) (vmcs->vmcs_vmexit_information_fields.exit_qualification));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rcx), (target_ulong) (vmcs->vmcs_vmexit_information_fields.io_rcx));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rsi), (target_ulong) (vmcs->vmcs_vmexit_information_fields.io_rsi));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr0), (target_ulong) (vmcs->vmcs_guest_state_area.cr0));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr3), (target_ulong) (vmcs->vmcs_guest_state_area.cr3));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr4), (target_ulong) (vmcs->vmcs_guest_state_area.cr4));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.es.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.cs.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.ss.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.ds.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.fs.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.gs.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.ldtr.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.tr.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.gdtr.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.gdtr.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.idtr.base_addr), (target_ulong) (vmcs->vmcs_guest_state_area.idtr.base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.dr7), (target_ulong) (vmcs->vmcs_guest_state_area.dr7));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.esp), (target_ulong) (vmcs->vmcs_guest_state_area.esp));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.eip), (target_ulong) (vmcs->vmcs_guest_state_area.eip));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.eflags), (target_ulong) (vmcs->vmcs_guest_state_area.eflags));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.pending_debug_exceptions), (target_ulong) (vmcs->vmcs_guest_state_area.pending_debug_exceptions_val));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_sysenter_esp), (target_ulong) (vmcs->vmcs_guest_state_area.msr_ia32_sysenter_esp));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_sysenter_eip), (target_ulong) (vmcs->vmcs_guest_state_area.msr_ia32_sysenter_eip));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.cr0), (target_ulong) (vmcs->vmcs_host_state_area.cr0));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.cr3), (target_ulong) (vmcs->vmcs_host_state_area.cr3));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.cr4), (target_ulong) (vmcs->vmcs_host_state_area.cr4));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.fs_base_addr), (target_ulong) (vmcs->vmcs_host_state_area.fs_base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.gs_base_addr), (target_ulong) (vmcs->vmcs_host_state_area.gs_base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.tr_base_addr), (target_ulong) (vmcs->vmcs_host_state_area.tr_base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.gdtr_base_addr), (target_ulong) (vmcs->vmcs_host_state_area.gdtr_base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.idtr_base_addr), (target_ulong) (vmcs->vmcs_host_state_area.idtr_base_addr));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_sysenter_esp), (target_ulong) (vmcs->vmcs_host_state_area.msr_ia32_sysenter_esp));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_sysenter_eip), (target_ulong) (vmcs->vmcs_host_state_area.msr_ia32_sysenter_eip));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.esp), (target_ulong) (vmcs->vmcs_host_state_area.esp));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_host_state_area.eip), (target_ulong) (vmcs->vmcs_host_state_area.eip));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rdi), (target_ulong) (vmcs->vmcs_vmexit_information_fields.io_rdi));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rip), (target_ulong) (vmcs->vmcs_vmexit_information_fields.io_rip));
	flush_field_tl(env, offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.guest_linear_addr), (target_ulong) (vmcs->vmcs_vmexit_information_fields.guest_linear_addr));


}

static void vm_exception(vm_exception_t exception, uint32_t err_number, CPUX86State * env){

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);
	struct vmcs_vmexit_information_fields * info = &(vmcs->vmcs_vmexit_information_fields);
	/* TODO */
 	/* define error numbers, see 30.4 */
	// Reference: 30.2 Conventions
	target_ulong update_mask;
	if (exception == SUCCEED){
		update_mask = CC_C | CC_P | CC_A | CC_S | CC_Z | CC_O;
		cpu_load_eflags(env, 0, update_mask);
	} else if (exception == FAIL_INVALID){
		LOG("FAIL INVALID");
		update_mask = CC_P | CC_A | CC_S | CC_Z | CC_O | CC_C;
		cpu_load_eflags(env, CC_C, update_mask);
	} else if(exception == FAIL_VALID){
		LOG("FAIL VALID");
		update_mask = CC_C | CC_P | CC_A | CC_S | CC_O | CC_Z;
		cpu_load_eflags(env, CC_Z, update_mask);
		// TODO: Set VM instruction error field to ErrNum
		info->instruction_error_field = err_number;
	}

}

// static void update_processor_vmcs(CPUX86State * env){

// 	CPUState *cs = CPU(x86_env_get_cpu(env));

// 	int ret = cpu_memory_rw_debug(cs, env->vmcs_ptr_register, env->processor_vmcs, sizeof(vtx_vmcs_t), 0 );

// 	printf("Update processor returned %d\n", ret);

// }

static void clear_address_range_monitoring(CPUX86State * env){
	/* Address range monitoring not implemented in QEMU */
	return;
}

#ifdef INCLUDE
static bool vmlaunch_check_vmx_controls_execution(CPUX86State * env){

	return true;
	
	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

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

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

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

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

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

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

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
#endif

static void vmlaunch_load_guest_state(CPUX86State * env){

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);
	struct vmcs_guest_state_area * g = &(vmcs->vmcs_guest_state_area);
	struct vmcs_vmentry_control_fields * cf = &(vmcs->vmcs_vmentry_control_fields);

	target_ulong reserved_mask;
	uint32_t new_crN;

	/* Loading Guest Control Registers, Debug Registers, and MSRs */
	//{
		/* load CR0 */
		reserved_mask = 0x7FFAFFD0;
		new_crN = (env->cr[0] & reserved_mask);
		new_crN |= (g->cr0 & ~reserved_mask);
		cpu_x86_update_cr0(env, new_crN);

		/* load cr3 & 4 */
		cpu_x86_update_cr3(env, g->cr3);
		cpu_x86_update_cr4(env, g->cr4);

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
			env->msr_global_ctrl = g->msr_ia32_perf_global_ctrl;
		}

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_PAT)){
			env->pat = g->msr_ia32_pat;
		}

		if (ISSET(cf->vmentry_controls, VM_ENTRY_LOAD_EFER)){
			//env->efer = g->msr_ia32_efer;
			cpu_load_efer(env, g->msr_ia32_efer);
		}
	//}

	/* Loading Guest Segment Registers and Descriptor-Table Registers */
	//{
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
			flags = access_rights.segment_type << DESC_TYPE_SHIFT; 	\
			flags |= access_rights.descriptor_type << 12; 	\
			flags |= access_rights.dpl << DESC_DPL_SHIFT; 	\
			flags |= access_rights.segment_present << 15; 	\
			flags |= access_rights.avl << 20; 	\
			flags |= access_rights.db << DESC_B_SHIFT; 	\
			flags |= access_rights.granularity << 23; 	\
		} while(0)	

		// load tr
		env->tr.selector = g->tr.selector;
		env->tr.base = g->tr.base_addr;
		env->tr.limit = g->tr.segment_limit;
		LOAD_ACCESS_RIGHTS(env->tr.flags, g->tr.access_rights);

		// load cs
		if (g->cs.access_rights.unusable){
			reserved_mask = DESC_G_MASK | DESC_B_MASK | DESC_L_MASK;
			env->segs[R_CS].flags &= ~reserved_mask;
			// NOTE: 64 bit needs L flag below
			env->segs[R_CS].flags |= (g->cs.access_rights.db << DESC_B_SHIFT) | (g->cs.access_rights.granularity << 23);
		} else {
			LOAD_ACCESS_RIGHTS(env->segs[R_CS].flags, g->cs.access_rights);
		}
		cpu_x86_load_seg_cache(env, R_CS, g->cs.selector, g->cs.base_addr, g->cs.segment_limit, env->segs[R_CS].flags);

		// SS, DS, ES, FS, GS, and LDTR
		if (g->ss.access_rights.unusable){

			env->segs[R_SS].base &= ~(0x0000000F);
			env->segs[R_SS].flags &= ~(DESC_DPL_MASK);
			env->segs[R_SS].flags |= g->ss.access_rights.dpl << DESC_DPL_SHIFT;
			env->segs[R_SS].flags |= (DESC_B_MASK);
			cpu_x86_load_seg_cache(env, R_SS, g->ss.selector, env->segs[R_SS].base, g->cs.segment_limit, env->segs[R_SS].flags);
		} else {
			LOAD_ACCESS_RIGHTS(env->segs[R_SS].flags, g->ss.access_rights);
			cpu_x86_load_seg_cache(env, R_SS, g->ss.selector, g->ss.base_addr, g->ss.segment_limit, env->segs[R_SS].flags);
		}

		env->segs[R_DS].selector = g->ds.selector;
		if (!g->ds.access_rights.unusable){
			env->segs[R_DS].base = g->ds.base_addr;
			env->segs[R_DS].limit = g->ds.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_DS].flags, g->ds.access_rights);
		}
		cpu_x86_load_seg_cache(env, R_DS, env->segs[R_DS].selector, env->segs[R_DS].base, env->segs[R_DS].limit, env->segs[R_DS].flags);

		env->segs[R_ES].selector = g->es.selector;
		if (!g->es.access_rights.unusable){
			env->segs[R_ES].base = g->es.base_addr;
			env->segs[R_ES].limit = g->es.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_ES].flags, g->es.access_rights);
		}
		cpu_x86_load_seg_cache(env, R_ES, env->segs[R_ES].selector, env->segs[R_ES].base, env->segs[R_ES].limit, env->segs[R_ES].flags);

		env->segs[R_FS].selector = g->fs.selector;
		env->segs[R_FS].base = g->fs.base_addr;
		if (!g->fs.access_rights.unusable){
			env->segs[R_FS].limit = g->fs.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_FS].flags, g->fs.access_rights);
			// some 64 bit stuff
		}
		cpu_x86_load_seg_cache(env, R_FS, env->segs[R_FS].selector, env->segs[R_FS].base, env->segs[R_FS].limit, env->segs[R_FS].flags);

		env->segs[R_GS].selector = g->gs.selector;
		env->segs[R_GS].base = g->gs.base_addr;
		if (!g->gs.access_rights.unusable){
			env->segs[R_GS].limit = g->gs.segment_limit;
			LOAD_ACCESS_RIGHTS(env->segs[R_GS].flags, g->gs.access_rights);
			// some 64 bit stuff
		}
		cpu_x86_load_seg_cache(env, R_GS, env->segs[R_GS].selector, env->segs[R_GS].base, env->segs[R_GS].limit, env->segs[R_GS].flags);

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
	//} 

	/* Loading Guest RIP, RSP, and RFLAGS */
	//{
		// watch out for 64 bit
		env->regs[R_ESP] = g->esp;
		env->eip = g->eip;
		// /env->eflags = g->eflags;
		cpu_load_eflags(env, g->eflags, 0xFFFFFFFF);
	//}

	// /* Loading Page-Directory-Pointer-Table Entries */
	// {
	// 	/* EPT stuff, not implemented */
	// }

	//  Updating Non-Register State 
	// {
	// 	/* EPT stuff, not implemented */

	// 	/* virtual interrupts, no idea what to do here */
	// }

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

	LOG_ENTRY;


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
			LOG("ILLEGAL OP EXCEPTION");
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_DISABLED){

		if (cpl > 0 ||
			/* cr0,4 values not supported */
			!ISSET(env->msr_ia32_feature_control, MSR_IA32_FEATURE_CONTROL_LOCK)
			/* smx */
			/* non smx */){
			printf("CPL = %d\n", cpl);
			printf("MSR = %ld\n", env->msr_ia32_feature_control);
			LOG("GENERAL PROTECTION EXCEPTION");
			raise_exception(env, EXCP0D_GPF);
		} else {
			/* check 4KB align or bits beyond phys range*/

			cpu_memory_rw_debug(cs, vmxon_region_ptr, (uint8_t *)&addr, sizeof(target_ulong), 0);

			if ((addr & 0xFFF) || (sizeof(target_ulong)*8 > TARGET_PHYS_ADDR_SPACE_BITS && addr >> TARGET_PHYS_ADDR_SPACE_BITS)){
				LOG("Not 4KB aligned, or bits set beyond range");
				vm_exception(FAIL_INVALID, 0, env);
			} else {
				
				rev =  (int32_t) x86_ldl_phys(cs, addr);
				/* TODO: cleanup, use structs for msr */
				if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) ||
					rev < 0 /* rev[31] = 1 */){
					LOG("rev[30:0] != MSR or rev[31] == 1");
					vm_exception(FAIL_INVALID, 0, env);
				} else {
					env->vmcs_ptr_register = VMCS_CLEAR_ADDRESS;
					env->vmx_operation = VMX_ROOT_OPERATION;
					/* TODO: block INIT signals */
					/* TODO: block A20M */
					//env->a20_mask = -1; // no change -- breaks everything if 1/0

					// TEMP FIXME
					env->processor_vmcs = (vtx_vmcs_t *) calloc(1, sizeof(vtx_vmcs_t));

					clear_address_range_monitoring(env);
					vm_exception(SUCCEED, 0, env);
					LOG("<1> In VMX Root Operation");
				}
			}

		}

	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* VM exit */
		LOG("VM EXIT");
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION");
		raise_exception(env, EXCP0D_GPF);
	} else {
		LOG("FAIL");;
		vm_exception(FAIL, 15, env);
	}

	LOG_EXIT;
}

void helper_vtx_vmxoff(CPUX86State * env){

	LOG_ENTRY;

	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED ||
		!ISSET(env->cr[0], CR0_PE_MASK) ||
		ISSET(env->eflags, VM_MASK) || 
		(ISSET(env->efer, MSR_EFER_LMA) && !ISSET(env->segs[R_CS].flags, DESC_L_MASK))) {
			LOG("INVALID OPCODE EXCEPTION");
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
		/* vm exit */
		LOG("VM EXIT");
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION");
		raise_exception(env, EXCP0D_GPF);
	// } else if (/* dual monitor - wtf is this? */){
		// vm_exception(FAIL, 23); // TO, envDO
	} else {
		LOG("DISABLING VMX");
		env->vmx_operation = VMX_DISABLED;
		/* unblock init */
		/* if ia32_smm_monitor[2] == 0, unblock SMIs */
		/* if outside SMX operration, unblock and enable A20M */
		//env->a20_mask = -1; // no change -- breaks everything if 1/0		
		clear_address_range_monitoring(env);
		vm_exception(SUCCEED,0, env);

		free(env->processor_vmcs);
		env->processor_vmcs = NULL;
	}


	LOG_EXIT;
}


void helper_vtx_vmptrld(CPUX86State * env, target_ulong vmcs_addr_phys){

	LOG_ENTRY;

	CPUState *cs = CPU(x86_env_get_cpu(env));
	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);
	
	target_ulong addr;
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

		cpu_memory_rw_debug(cs, vmcs_addr_phys, (uint8_t *)&addr, sizeof(target_ulong), 0);

		if ((addr & 0xFFF) || ( sizeof(addr)*8 > TARGET_PHYS_ADDR_SPACE_BITS && addr >> 32)) {
			LOG("FAIL - 9");
			vm_exception(FAIL, 9, env);
		} else if (addr == (uint64_t) env->vmxon_ptr_register){
			LOG("FAIL - 10");
			vm_exception(FAIL, 10, env);
		} else {
			rev = (int32_t) x86_ldl_phys(cs, addr);
			if ((rev & 0x7FFFFFFF) != (env->msr_ia32_vmx_basic & 0x7FFFFFFF) or
				(rev < 0 /* processor does not support 1 setting of VMCS shadowing*/)){
				LOG("FAIL - 11");
				vm_exception(FAIL, 11, env); // to, envdo
			} else {
				env->vmcs_ptr_register = addr;
				vmcs->revision_identifier = env->msr_ia32_vmx_basic & 0x7FFFFFFF;
				printf("Set vmcs ptr to %d\n", addr);
				//load_vmcs_proc(addr);
				vm_exception(SUCCEED, 0, env);
			}
		}
	}

	LOG_EXIT;

}

void helper_vtx_invept(CPUX86State * env, target_ulong reg, uint64_t mem){
	LOG_ENTRY;
	LOG_EXIT;
}

void helper_vtx_invvpid(CPUX86State * env, target_ulong reg, uint64_t mem){
	LOG_ENTRY;
	LOG_EXIT;
}

void helper_vtx_vmcall(CPUX86State * env){

	// int cpl = env->segs[R_CS].selector & 0x3;

	// if (env->vmx_operation == VMX_DISABLED){
	// 	raise_exception(env, EXCP06_ILLOP);
	// } else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
	// 	/* vm exit */
	// } else if (ISSET(env->eflags, VM_MASK) or (ISSET(env->efer,MSR_EFER_LMA) and  !ISSET(env->segs[R_CS].flags, DESC_L_MASK)) ){
	// 	raise_exception(env, EXCP06_ILLOP);
	// } else if (cpl > 0){
	// 	raise_exception(env, EXCP0D_GPF);
	// } else if (SMM or no dual treatment or !ISSET(env, /* IA32_SMM_MONITOR_CTL */)){
	// 	vm_exception(FAIL, 1, env);
	// } else if (/*dual treatment of of SMIs and SMM is active*/){
	// 	/* VMM VM exit? */
	// } else if (env->vmcs_ptr_register == NULL or env->vmcs_ptr_register == VMCS_CLEAR_ADDRESS){
	// 	vm_exception(FAIL_INVALID, 0, env);
	// } else if (env->processor_vmcs.launch_state != LAUNCH_STATE_CLEAR){
	// 	vm_exception(FAIL, 1, env)
	// } else if (/* vm exit control fields invalid */){
	// 	vm_exception(FAIL, 20, env);
	// } else {

	// 	/* enter SMM */
	// 	int rev = /* revision identfier in MSEG */
	// 	if (rev is unsupported){
	// 		/* leave SMM */
	// 		vm_exception(FAIL, 22, env);
	// 	} else {
	// 		int smm_features = /* from MSEG */
	// 		if (smm_features is invalid){
	// 			/*leave SMM */
	// 			vm_exception(FAIL, 24, env);
	// 		} else {
	// 			/* activate dual monitor treatment of SMIs and SMM */
	// 		}
	// 	}

	// }
	LOG_ENTRY;
	LOG_EXIT;
}

void helper_vtx_vmclear(CPUX86State * env, target_ulong vmcs_addr_phys){

	LOG_ENTRY;

	CPUState *cs = CPU(x86_env_get_cpu(env));

	int cpl = env->segs[R_CS].selector & 0x3;

	target_ulong addr;

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

		cpu_memory_rw_debug(cs, vmcs_addr_phys, (uint8_t *)&addr, sizeof(target_ulong), 0);

		if ((addr & 0xFFF) or (sizeof(addr)*8 > TARGET_PHYS_ADDR_SPACE_BITS && addr >> TARGET_PHYS_ADDR_SPACE_BITS)){
			LOG("FAIL - 2");
			vm_exception(FAIL, 2, env);
		} else if (addr == env->vmxon_ptr_register){
			LOG("FAIL - 3");
			vm_exception(FAIL, 3, env);
		} else {

			/* TODO -- ensure that data for VMCS references by operand is in memory */
			/* TODO -- initialize V<CS region */
			x86_stl_phys(cs, addr + offsetof(vtx_vmcs_t, launch_state), LAUNCH_STATE_CLEAR);
			// sets shadow_vmcs_indicator to 0 as well, just because width is 32
			// we dont use offsetof here since that would produce bitfield ptr error
			// luckily, revision_identifier is the first element in the struct
			x86_stl_phys(cs, addr, MSR_IA32_VMX_BASIC_DEFAULT & 0x7FFFFFFF);
			if ((uint64_t)(env->vmcs_ptr_register) == addr){
				LOG("Resetting vmcs_ptr_register");
				env->vmcs_ptr_register = VMCS_CLEAR_ADDRESS;
			}
			LOG("VMCS initialized");
			vm_exception(SUCCEED, 0, env);
		}
	}


	LOG_EXIT;

}


void helper_vtx_vmfunc(CPUX86State * env){
	LOG_ENTRY;
	LOG_EXIT;
	//target_ulong eax = env->regs[R_EAX];

	/* only used for EPTP switching, not implementing for now */

}


void helper_vtx_vmlaunch(CPUX86State * env){
	
	LOG_ENTRY;

	int cpl = env->segs[R_CS].selector & 0x3;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			LOG("ILLEGAL OPCODE EXCEPTION");
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){

		LOG("VM EXIT");
		/* vm exit */
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION");
		raise_exception(env, EXCP0D_GPF);
	} else if (vmcs == NULL or  (uint64_t)vmcs == VMCS_CLEAR_ADDRESS){

		LOG("INVALID/UNINITIALIZED VMCS");
		vm_exception(FAIL_INVALID, 0, env);
	// } else if (/* events blocked by MOV SS */){
		// vm_exception(FAIL, 26, env);
	} else if (vmcs->launch_state != LAUNCH_STATE_CLEAR){
		LOG("ERROR: VM LAUNCH STATE NOT CLEAR");
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
		
		vmlaunch_load_guest_state(env);
		clear_address_range_monitoring(env);
		vmlaunch_load_msrs(env);
		vmcs->launch_state = LAUNCH_STATE_LAUNCHED;
		env->vmx_operation = VMX_NON_ROOT_OPERATION;
		flush_active_vmcs(env);
		vm_exception(SUCCEED, 0, env);
	}


	LOG_EXIT;

}

void cpu_vmx_check_exception(CPUX86State * env, int intno, int error_code, int next_eip_addend, uintptr_t retaddr){

	CPUState *cs = CPU(x86_env_get_cpu(env));

	if (env->vmx_operation != VMX_NON_ROOT_OPERATION)
		return;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

	struct vmcs_vmexecution_control_fields * exec_cf = &(vmcs->vmcs_vmexecution_control_fields);
	struct vmcs_vmexit_information_fields * exit_info = &(vmcs->vmcs_vmexit_information_fields);
	uint32_t exception_bitmap = exec_cf->exception_bitmap;

	struct vmcs_vmexit_information_fields fields;
	memset(&fields, 0, sizeof(struct vmcs_vmexit_information_fields));

	fields.exit_reason.basic_reason = (uint16_t) VTX_EXIT_EXCP_NMI;
	fields.interruption_info.vector = intno;
	// Is 0 an error code? QEMU doesn't seem to think so
	fields.interruption_info.error_code_valid = (error_code) ? 1 : 0; 
	fields.interruption_info.type = 3;
	fields.interruption_error_code = error_code;
	fields.interruption_info.valid = 1;

	/* assign specific values for vmexit info */
	switch (intno){
	case EXCP01_DB:
		fields.exit_qualification = 0; // table 27-1
		fields.interruption_info.type = 6;
		break;
	case EXCP04_INTO:
		fields.interruption_info.type = 6;
		break;
	case EXCP0E_PAGE:
		// see x86_cpu_handle_mmu_fault in helper.c
		fields.exit_qualification = exit_info->exit_qualification;
		break;
	default:
		break;
	}

	/* determine valid conditions for VMexits */
	if (intno != EXCP0E_PAGE && ISSET(exception_bitmap, 1UL << intno)){
		vtx_vmexit(env, &fields, next_eip_addend);
	} else {
		// special conditions for handling page faults
		if ((error_code & exec_cf->page_fault_error_code_mask) == exec_cf->page_fault_error_code_match){
			LOG("PF exception due to PFEC match");
			if (ISSET(exception_bitmap, 1UL << intno)) 
				vtx_vmexit(env, &fields, next_eip_addend);
		} else {
			if (!ISSET(exception_bitmap, 1UL << intno)) 
				vtx_vmexit(env, &fields, next_eip_addend);
		}
	}



	/* remove any pending exception */
    	cs->exception_index = -1;
    	env->error_code = 0;
    	env->old_exception = -1;
	cpu_loop_exit(cs);

}

void cpu_vmx_check_intercept(CPUX86State * env, uint32_t basic_reason, target_ulong next_eip, target_ulong eflags, int error_code){

	CPUState *cs = CPU(x86_env_get_cpu(env));

	if (env->vmx_operation != VMX_NON_ROOT_OPERATION)
		return;

	struct vmcs_vmexit_information_fields fields;
	memset(&fields, 0, sizeof(struct vmcs_vmexit_information_fields));

	fields.exit_reason.basic_reason = (uint16_t) basic_reason;
	fields.exit_qualification = env->cr[2];
	fields.interruption_info.vector = 14;
	fields.interruption_info.type = 3;
	fields.interruption_info.error_code_valid = 1;
	fields.interruption_info.valid = 1;

	fields.interruption_error_code = error_code;



	vtx_vmexit(env, &fields, next_eip);

	/* remove any pending exception */
    	cs->exception_index = -1;
    	env->error_code = 0;
    	env->old_exception = -1;

  

	cpu_loop_exit(cs);
}

static void vtx_vmexit(CPUX86State * env, struct vmcs_vmexit_information_fields * fields, target_ulong eip){

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

	struct vmcs_guest_state_area * g = &(vmcs->vmcs_guest_state_area);
	struct vmcs_host_state_area * h = &(vmcs->vmcs_host_state_area);
	uint32_t vmexit_controls = vmcs->vmcs_vmexit_control_fields.vmexit_controls;
	
	/* 27.2 record exit info */
	// TODO: some other stuff in 27.2
	memcpy(&(vmcs->vmcs_vmexit_information_fields), fields, sizeof(struct vmcs_vmexit_information_fields));

	/* save guest state */

	g->cr0 = env->cr[0];
	g->cr3 = env->cr[3];
	g->cr4 = env->cr[4];
	g->msr_sysenter_cs = env->sysenter_cs;
	g->msr_ia32_sysenter_esp = env->sysenter_esp;
	g->msr_ia32_sysenter_eip = env->sysenter_eip;

	if (ISSET(vmexit_controls, VM_EXIT_SAVE_DEBUG_CONTROLS)){
		g->dr7 = env->dr[7];
		g->msr_ia32_debugctl = env->msr_ia32_debugctl;
	}


	if (ISSET(vmexit_controls, VM_EXIT_SAVE_PAT)){
		g->msr_ia32_pat = env->pat;
	}

	if (ISSET(vmexit_controls, VM_EXIT_SAVE_EFER)){
		g->msr_ia32_efer = env->efer;
	}

	// save segments
	g->es.selector = (uint16_t) env->segs[R_ES].selector;
	g->cs.selector = (uint16_t) env->segs[R_CS].selector;
	g->ss.selector = (uint16_t) env->segs[R_SS].selector;
	g->ds.selector = (uint16_t) env->segs[R_DS].selector;
	g->fs.selector = (uint16_t) env->segs[R_FS].selector;
	g->gs.selector = (uint16_t) env->segs[R_GS].selector;
	g->ldtr.selector = (uint16_t) env->ldt.selector;
	g->tr.selector = (uint16_t) env->tr.selector;

	#define SAVE_EXIT_ACCESS_RIGHTS(ar, flags) do { 	\
		ar.segment_type = (flags & DESC_TYPE_MASK) >> DESC_TYPE_SHIFT;	\
		ar.descriptor_type = (flags & DESC_S_MASK) >> 12;				\
		ar.dpl = (flags & DESC_DPL_MASK) >> DESC_DPL_SHIFT;				\
		ar.segment_present = (flags & DESC_P_MASK) >> 15;				\
		ar.reserved0 = 0;												\
		ar.avl = (flags & DESC_AVL_MASK) >> 20;							\
		ar.reserved1 = 0;												\
		ar.db = (flags & DESC_B_MASK) >> DESC_B_SHIFT; 					\
		ar.granularity = (flags & DESC_G_MASK) >> 23;					\
		ar.unusable = 0;												\
		ar.reserved2 = 0;												\
	} while (0)

	g->es.access_rights_val = 0;
	if (ISSET(env->segs[R_ES].flags, 1UL << 16)){
		g->es.access_rights.unusable = 1;
	} else {
		g->es.base_addr = env->segs[R_ES].base;
		g->es.segment_limit = env->segs[R_ES].limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->es.access_rights, env->segs[R_ES].flags);
	}
	// 64 bit stuff

	g->cs.access_rights_val = 0;
	g->cs.base_addr = env->segs[R_CS].base;
	g->cs.segment_limit = env->segs[R_CS].limit;
	if (ISSET(env->segs[R_CS].flags, 1UL << 16)){
		g->cs.access_rights.unusable = 1;
	} else {
		SAVE_EXIT_ACCESS_RIGHTS(g->cs.access_rights, env->segs[R_CS].flags);
	}
	g->cs.access_rights.avl = likely(env->segs[R_CS].flags & (1UL << 20));
	g->cs.access_rights.db = likely(env->segs[R_CS].flags & (1UL << DESC_B_SHIFT));
	g->cs.access_rights.granularity = likely(env->segs[R_CS].flags & (1UL << 23));

	g->ss.access_rights_val = 0;
	if (ISSET(env->segs[R_SS].flags, 1UL << 16)){
		g->ss.access_rights.unusable = 1;
	} else {
		g->ss.base_addr = env->segs[R_SS].base;
		g->ss.segment_limit = env->segs[R_SS].limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->ss.access_rights, env->segs[R_SS].flags);
	}
	g->ss.access_rights.dpl = (env->segs[R_SS].flags & DESC_DPL_MASK) >> DESC_DPL_SHIFT;
	// 64 bit stuff


	g->ds.access_rights_val = 0;
	if (ISSET(env->segs[R_DS].flags, 1UL << 16)){
		g->ds.access_rights.unusable = 1;
	} else {
		g->ds.base_addr = env->segs[R_DS].base;
		g->ds.segment_limit = env->segs[R_DS].limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->ds.access_rights, env->segs[R_DS].flags);
	}
	// 64 bit stuff

	g->fs.access_rights_val = 0;
	if (ISSET(env->segs[R_FS].flags, 1UL << 16)){
		g->fs.access_rights.unusable = 1;
	} else {
		g->fs.segment_limit = env->segs[R_FS].limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->fs.access_rights, env->segs[R_FS].flags);
	}
	g->fs.base_addr = env->segs[R_FS].base;

	g->ldtr.access_rights_val = 0;
	if (ISSET(env->ldt.flags, 1UL << 16)){
		g->ldtr.access_rights.unusable = 1;
	} else {
		g->ldtr.segment_limit = env->ldt.limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->ldtr.access_rights, env->ldt.flags);
	}
	g->ldtr.base_addr = env->ldt.base;

	g->gs.access_rights_val = 0;
	if (ISSET(env->segs[R_GS].flags, 1UL << 16)){
		g->gs.access_rights.unusable = 1;
	} else {
		g->gs.segment_limit = env->segs[R_GS].limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->gs.access_rights, env->segs[R_GS].flags);
	}
	g->gs.base_addr = env->segs[R_GS].base;

	g->tr.access_rights_val = 0;
	if (ISSET(env->tr.flags, 1UL << 16)){
		g->tr.access_rights.unusable = 1;
	} else {
		g->tr.base_addr = env->tr.base;
		g->tr.segment_limit = env->tr.limit;
		SAVE_EXIT_ACCESS_RIGHTS(g->tr.access_rights, env->tr.flags);
	}

	g->gdtr.base_addr = env->gdt.base;
	g->gdtr.limit = env->gdt.limit;

	g->idtr.base_addr = env->idt.base;
	g->idtr.limit = env->idt.limit;

	// save eip, eflags and esp
	g->eip = eip;
	g->esp = env->regs[R_ESP];
	g->eflags = env->eflags;
	cpu_load_eflags(env, 0x1, ~(CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C | DF_MASK |
                      VM_MASK));

	g->activity_state = ACTIVITY_STATE_ACTIVE; // TODO
	g->interruptibility_state = INTERRUPTIBILITY_STIBLOCK; // TODO

	// TODO: pending debug exceptions

	if (ISSET(vmexit_controls, VM_EXIT_SAVE_PREEMPT_TIMER)){
		g->vmx_preemption_timer = 0xFFFFFFFF; // todo
	}

	// TODO: EPT stuff

	// TODO: MSRs? Gotta save em in vmlaunch first

	// Load host state
	uint32_t reserved_mask = 0x7FFAFFD0;
	target_ulong new_crN = (env->cr[0] & reserved_mask);
	new_crN |= (h->cr0 & ~reserved_mask);
	cpu_x86_update_cr0(env, new_crN);
	
	cpu_x86_update_cr3(env, h->cr3);
	cpu_x86_update_cr4(env, h->cr4);
	// more modifications to cr3 c4 TODO

	env->dr[7] = 0x400;

	env->msr_ia32_debugctl = 0;
	env->sysenter_cs = h->msr_sysenter_cs;
	env->sysenter_esp = h->msr_ia32_sysenter_esp;
	env->sysenter_eip = h->msr_ia32_sysenter_eip;
	// some 64 bit stuff

	// more 64 bit stuff

	if (ISSET(vmexit_controls, VM_EXIT_LD_PERF_GLOB)){
		env->msr_global_ctrl = h->msr_ia32_perf_global_ctrl;
		// TODO: ensure reserved bits
	}

	if (ISSET(vmexit_controls, VM_EXIT_LOAD_PAT)){
		env->pat = h->msr_ia32_pat;
		// TODO: ensure reserved bits
	}

	if (ISSET(vmexit_controls, VM_EXIT_LOAD_EFER)){
		cpu_load_efer(env, h->msr_ia32_efer);
	}

	// TODO: something about MSRs overwritten

	// load segment registers

	// TODO: 64 bit stuff

	env->segs[R_CS].selector = h->cs_selector;
	env->segs[R_CS].base = 0x0;
	env->segs[R_CS].limit = 0xFFFFFFFF;
	env->segs[R_CS].flags = (0xB << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_G_MASK;
	if (!ISSET(vmexit_controls, VM_EXIT_HOST_ADDR_SPACE_SIZE)){
		env->segs[R_CS].flags |= DESC_B_MASK;
	}

	cpu_x86_load_seg_cache(env, R_CS, 
						   env->segs[R_CS].selector, 
						   env->segs[R_CS].base,
						   env->segs[R_CS].limit,
						   env->segs[R_CS].flags);

	env->segs[R_SS].selector = h->ss_selector;
	// TODO: 64 bit stuff, unusable if 0
	env->segs[R_SS].base = 0x0;
	env->segs[R_SS].limit = 0xFFFFFFFF;
	env->segs[R_SS].flags = (0x3 << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_B_MASK | DESC_G_MASK;
	cpu_x86_load_seg_cache(env, R_SS, 
						   env->segs[R_SS].selector, 
						   env->segs[R_SS].base,
						   env->segs[R_SS].limit,
						   env->segs[R_SS].flags);

	env->segs[R_DS].selector = h->ds_selector;
	if (h->ds_selector != 0){
		env->segs[R_DS].base = 0x0;
		env->segs[R_DS].limit = 0xFFFFFFFF;
		env->segs[R_DS].flags = (0x3 << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_B_MASK | DESC_G_MASK;
	}
	cpu_x86_load_seg_cache(env, R_DS, 
						   env->segs[R_DS].selector, 
						   env->segs[R_DS].base,
						   env->segs[R_DS].limit,
						   env->segs[R_DS].flags);

	env->segs[R_ES].selector = h->es_selector;
	if (h->es_selector != 0){
		env->segs[R_ES].base = 0x0;
		env->segs[R_ES].limit = 0xFFFFFFFF;
		env->segs[R_ES].flags = (0x3 << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_B_MASK | DESC_G_MASK;
	}
	cpu_x86_load_seg_cache(env, R_ES, 
						   env->segs[R_ES].selector, 
						   env->segs[R_ES].base,
						   env->segs[R_ES].limit,
						   env->segs[R_ES].flags);

	env->segs[R_FS].selector = h->fs_selector;
	if (h->fs_selector != 0){
		env->segs[R_FS].base = h->fs_base_addr;
		// more TODO 64 bit stuff
		env->segs[R_FS].limit = 0xFFFFFFFF;
		env->segs[R_FS].flags = (0x3 << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_B_MASK | DESC_G_MASK;
	}
	cpu_x86_load_seg_cache(env, R_FS, 
						   env->segs[R_FS].selector, 
						   env->segs[R_FS].base,
						   env->segs[R_FS].limit,
						   env->segs[R_FS].flags);

	env->segs[R_GS].selector = h->gs_selector;
	if (h->gs_selector != 0){
		env->segs[R_GS].base = h->gs_base_addr;
		// more TODO 64 bit stuff
		env->segs[R_GS].limit = 0xFFFFFFFF;
		env->segs[R_GS].flags = (0x3 << DESC_TYPE_SHIFT) | DESC_S_MASK | DESC_P_MASK | DESC_B_MASK | DESC_G_MASK;
	}
	cpu_x86_load_seg_cache(env, R_GS, 
						   env->segs[R_GS].selector, 
						   env->segs[R_GS].base,
						   env->segs[R_GS].limit,
						   env->segs[R_GS].flags);

	env->tr.selector = h->tr_selector;
	env->tr.base = h->tr_base_addr;
	env->tr.limit = 0x00000067;
	env->tr.flags = (0xB << DESC_TYPE_SHIFT) | DESC_P_MASK;

	env->ldt.selector = 0x0;
	// remedy this to unsuable
	//env->ldt.flags |= (1UL << 16); // mark unusable

	env->gdt.base = h->gdtr_base_addr;
	env->gdt.limit = 0xFFFF;
	env->idt.base = h->idtr_base_addr;
	env->idt.limit = 0xFFFF;
	// TODO: more 64 bit stuff

	// load eip, esp, eflags
	env->eip = h->eip;
	env->regs[R_ESP] = h->esp;
	env->eflags = 0x2;

	// paging stuff, might be important TODO


	clear_address_range_monitoring(env);

	flush_active_vmcs(env);

	// this was skipped in the manual, but might be importatn
	env->vmx_operation = VMX_ROOT_OPERATION;
	vm_exception(SUCCEED,0, env);

	LOG_EXIT;

}


void helper_vtx_vmresume(CPUX86State * env){


	LOG_ENTRY;

	int cpl = env->segs[R_CS].selector & 0x3;

	vtx_vmcs_t * vmcs = (vtx_vmcs_t *) (env->processor_vmcs);

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			LOG("ILLEGAL OPCODE EXCEPTION");
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){

		LOG("VM EXIT");
		/* vm exit */
	} else if (cpl > 0){
		LOG("GENERAL PROTECTION EXCEPTION");
		raise_exception(env, EXCP0D_GPF);
	} else if (vmcs == NULL or  (uint64_t)vmcs == VMCS_CLEAR_ADDRESS){

		LOG("INVALID/UNINITIALIZED VMCS");
		vm_exception(FAIL_INVALID, 0, env);
	// } else if (/* events blocked by MOV SS */){
		// vm_exception(FAIL, 26, env);
	} else if (vmcs->launch_state != LAUNCH_STATE_LAUNCHED){
		LOG("ERROR: VM RESUME with non-launched VMCS");
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
		
		vmlaunch_load_guest_state(env);
		clear_address_range_monitoring(env);
		vmlaunch_load_msrs(env);
		env->vmx_operation = VMX_NON_ROOT_OPERATION;
		flush_active_vmcs(env);

		if (vmcs->vmcs_vmentry_control_fields.interruption_info.valid){
			LOG("Need to inject event");
			printf("injection type = %d\n", vmcs->vmcs_vmentry_control_fields.interruption_info.type);
		}

		vm_exception(SUCCEED, 0, env);
	}


	LOG_EXIT;
}

void helper_vtx_vmptrst(CPUX86State * env, target_ulong vmcs_addr_phys){

	// int cpl = env->segs[R_CS].selector & 0x3;

	// /* again, check DESC_L_MASK 64 bit only ? */
	// if (env->vmx_operation == VMX_DISABLED or
	// 	!ISSET(env->cr[0], CR0_PE_MASK) or
	// 	ISSET(env->eflags, VM_MASK) or 
	// 	(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
	// 		raise_exception(env, EXCP06_ILLOP);
	// } else if (env->vmx_operation == VMX_NON_ROOT_OPERATION){
	// 	/* vm exit */
	// } else if (cpl > 0){
	// 	raise_exception(env, EXCP0D_GPF);
	// } else {
	// 	*dest = (uint64_t) env->vmcs;
	// 	vm_exception(SUCCEED,0, env);
	// }
	LOG_ENTRY;
	while(1);
	LOG_EXIT;
}

static int32_t get_vmcs_offset16(target_ulong vmcs_field_encoding, int32_t is_write){

	switch ((vmcs_field_encoding >> 1) << 1){
		case 0x000: PRINT(vmcs_vmexecution_control_fields.vpid); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vpid);
		case 0x002: PRINT(vmcs_vmexecution_control_fields.posted_interrupt_notification_vector); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.posted_interrupt_notification_vector);
		case 0x004: PRINT(vmcs_vmexecution_control_fields.eptp_index); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp_index);
			
		case 0x800: PRINT(vmcs_guest_state_area.es.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.selector);
		case 0x802: PRINT(vmcs_guest_state_area.cs.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.selector);
		case 0x804: PRINT(vmcs_guest_state_area.ss.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.selector);
		case 0x806: PRINT(vmcs_guest_state_area.ds.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.selector);
		case 0x808: PRINT(vmcs_guest_state_area.fs.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.selector);
		case 0x80A: PRINT(vmcs_guest_state_area.gs.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.selector);
		case 0x80C: PRINT(vmcs_guest_state_area.ldtr.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.selector);
		case 0x80E: PRINT(vmcs_guest_state_area.tr.selector); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.selector);
		case 0x810: PRINT(vmcs_guest_state_area.guest_interrupt_status); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.guest_interrupt_status);
			
		case 0xC00: PRINT(vmcs_host_state_area.es_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.es_selector);
		case 0xC02: PRINT(vmcs_host_state_area.cs_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.cs_selector);
		case 0xC04: PRINT(vmcs_host_state_area.ss_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.ss_selector);
		case 0xC06: PRINT(vmcs_host_state_area.ds_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.ds_selector);
		case 0xC08: PRINT(vmcs_host_state_area.fs_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.fs_selector);
		case 0xC0A: PRINT(vmcs_host_state_area.gs_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.gs_selector);
		case 0xC0C: PRINT(vmcs_host_state_area.tr_selector); return offsetof(vtx_vmcs_t, vmcs_host_state_area.tr_selector);
	
		default:
			return -1;

	}
}

static int32_t get_vmcs_offset64(target_ulong vmcs_field_encoding, int32_t is_write){

	switch ((vmcs_field_encoding >> 1) << 1){
		case 0x2000: 
		 PRINT(vmcs_vmexecution_control_fields.io_bitmap_addr_A); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.io_bitmap_addr_A);  
		case 0x2002: 
		 PRINT(vmcs_vmexecution_control_fields.io_bitmap_addr_B); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.io_bitmap_addr_B); 
		case 0x2004: 
		 PRINT(vmcs_vmexecution_control_fields.read_bitmap_low_msr); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_bitmap_low_msr); 
		case 0x2006: 
		 PRINT(vmcs_vmexit_control_fields.msr_store_addr); return offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_store_addr); 
		case 0x2008: 
		 PRINT(vmcs_vmexit_control_fields.msr_load_addr); return offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_load_addr);
		case 0x200A: 
		 PRINT(vmcs_vmentry_control_fields.msr_load_addr); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.msr_load_addr);
		case 0x200C: 
		 PRINT(vmcs_vmexecution_control_fields.executive_vmcs_pointer); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.executive_vmcs_pointer);
		case 0x2010: 
		 PRINT(vmcs_vmexecution_control_fields.tsc_offset); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.tsc_offset);
		case 0x2012: 
		 PRINT(vmcs_vmexecution_control_fields.virt_apic_address); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.virt_apic_address);
		case 0x2014: 
		 PRINT(vmcs_vmexecution_control_fields.apic_access_address); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.apic_access_address);
		case 0x2016: 
		 PRINT(vmcs_vmexecution_control_fields.posted_interrupt_descriptor_addr); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.posted_interrupt_descriptor_addr);
		case 0x2018: 
		 PRINT(vmcs_vmexecution_control_fields.vm_function_control_vector); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vm_function_control_vector);
		case 0x201A: 
		 PRINT(vmcs_vmexecution_control_fields.eptp); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp);
		case 0x201C: 
		 PRINT(vmcs_vmexecution_control_fields.eoi_exit_bitmap[0]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[0]);
		case 0x201E: 
		 PRINT(vmcs_vmexecution_control_fields.eoi_exit_bitmap[1]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[1]);
		case 0x2020: 
		 PRINT(vmcs_vmexecution_control_fields.eoi_exit_bitmap[2]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[2]);
		case 0x2022: 
		 PRINT(vmcs_vmexecution_control_fields.eoi_exit_bitmap[3]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eoi_exit_bitmap[3]);
		case 0x2024: 
		 PRINT(vmcs_vmexecution_control_fields.eptp_list_address); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.eptp_list_address);
		case 0x2026: 
		 PRINT(vmcs_vmexecution_control_fields.vmread_bitmap); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vmread_bitmap);
		case 0x2028: 
		 PRINT(vmcs_vmexecution_control_fields.vmwrite_bitmap); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.vmwrite_bitmap);
		case 0x202A: 
		 PRINT(vmcs_vmexecution_control_fields.virt_exception_info_addr); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.virt_exception_info_addr);
		case 0x202C: 
		 PRINT(vmcs_vmexecution_control_fields.xss_exiting_bitmap); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.xss_exiting_bitmap);

		/* read only field */
		case 0x2400:
			if (!is_write){
			 PRINT(vmcs_vmexit_information_fields.guest_phys_addr); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.guest_phys_addr);
			} else {
				return -2;
			}
		
		case 0x2800:
		 PRINT(vmcs_guest_state_area.vmcs_link_ptr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.vmcs_link_ptr);
		case 0x2802:
		 PRINT(vmcs_guest_state_area.msr_ia32_debugctl); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_debugctl);
		case 0x2804:
		 PRINT(vmcs_guest_state_area.msr_ia32_pat); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_pat);
		case 0x2806:
		 PRINT(vmcs_guest_state_area.msr_ia32_efer); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_efer);
		case 0x2808:
		 PRINT(vmcs_guest_state_area.msr_ia32_perf_global_ctrl); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_perf_global_ctrl);
		case 0x280A:
		 PRINT(vmcs_guest_state_area.pdpte[0]); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[0]);
		case 0x280C:
		 PRINT(vmcs_guest_state_area.pdpte[1]); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[1]);
		case 0x280E:
		 PRINT(vmcs_guest_state_area.pdpte[2]); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[2]);
		case 0x2810:
		 PRINT(vmcs_guest_state_area.pdpte[3]); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.pdpte[3]);

		case 0x2C00:
		 PRINT(vmcs_host_state_area.msr_ia32_pat); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_pat);
		case 0x2C02:
		 PRINT(vmcs_host_state_area.msr_ia32_efer); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_efer);
		case 0x2C04:
		 PRINT(vmcs_host_state_area.msr_ia32_perf_global_ctrl); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_perf_global_ctrl);


		default:
		break;

	}
	LOG("Encoding not found");
	printf("Encoding = %d\n", vmcs_field_encoding);
	return -1;
}	

static int32_t get_vmcs_offset32(target_ulong vmcs_field_encoding, int32_t is_write){


	switch ((vmcs_field_encoding >> 1) << 1){
		case 0x4000: PRINT(vmcs_vmexecution_control_fields.async_event_control); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.async_event_control);
		case 0x4002: PRINT(vmcs_vmexecution_control_fields.primary_control); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.primary_control);
		case 0x4004: PRINT(vmcs_vmexecution_control_fields.exception_bitmap); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.exception_bitmap);
		case 0x4006: PRINT(vmcs_vmexecution_control_fields.page_fault_error_code_mask); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.page_fault_error_code_mask);
		case 0x4008: PRINT(vmcs_vmexecution_control_fields.page_fault_error_code_match); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.page_fault_error_code_match);
		case 0x400A: PRINT(vmcs_vmexecution_control_fields.cr3_target_count); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target_count);
		case 0x400C: PRINT(vmcs_vmexit_control_fields.vmexit_controls); return offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.vmexit_controls);
		case 0x400E: PRINT(vmcs_vmexit_control_fields.msr_store_count); return offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_store_count);
		case 0x4010: PRINT(vmcs_vmexit_control_fields.msr_load_count); return offsetof(vtx_vmcs_t, vmcs_vmexit_control_fields.msr_load_count);
		case 0x4012: PRINT(vmcs_vmentry_control_fields.vmentry_controls); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.vmentry_controls);
		case 0x4014: PRINT(vmcs_vmentry_control_fields.msr_load_count); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.msr_load_count);
		case 0x4016: PRINT(vmcs_vmentry_control_fields.interruption_info); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.interruption_info);
		case 0x4018: PRINT(vmcs_vmentry_control_fields.exception_err_code); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.exception_err_code);
		case 0x401A: PRINT(vmcs_vmentry_control_fields.instruction_length); return offsetof(vtx_vmcs_t, vmcs_vmentry_control_fields.instruction_length);
		case 0x401C: PRINT(vmcs_vmexecution_control_fields.tpr_threshold); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.tpr_threshold);
		case 0x401E: PRINT(vmcs_vmexecution_control_fields.secondary_control); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.secondary_control);
		case 0x4020: PRINT(vmcs_vmexecution_control_fields.ple_gap); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.ple_gap);
		case 0x4022: PRINT(vmcs_vmexecution_control_fields.ple_window); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.ple_window);
		
		case 0x4400: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.instruction_error_field); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_error_field);
			}
			else
				return -2;
		case 0x4402: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.exit_reason); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.exit_reason);
			}
			else
				return -2;
		case 0x4404: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.interruption_info); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.interruption_info);
			}
			else
				return -2;
		case 0x4406: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.interruption_error_code); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.interruption_error_code);
			}
			else
				return -2;
		case 0x4408: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.idt_vectoring_info); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.idt_vectoring_info);
			}
			else
				return -2;
		case 0x440A: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.idt_vectoring_err_code); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.idt_vectoring_err_code);
			}
			else
				return -2;
		case 0x440C: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.instruction_length); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_length);
			}
			else
				return -2;
		case 0x440E: 
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.instruction_info); return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.instruction_info);
			}
			else
				return -2;

		case 0x4800: PRINT(vmcs_guest_state_area.es.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.segment_limit);
		case 0x4802: PRINT(vmcs_guest_state_area.cs.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.segment_limit);
		case 0x4804: PRINT(vmcs_guest_state_area.ss.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.segment_limit);
		case 0x4806: PRINT(vmcs_guest_state_area.ds.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.segment_limit);
		case 0x4808: PRINT(vmcs_guest_state_area.fs.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.segment_limit);
		case 0x480A: PRINT(vmcs_guest_state_area.gs.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.segment_limit);
		case 0x480C: PRINT(vmcs_guest_state_area.ldtr.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.segment_limit);
		case 0x480E: PRINT(vmcs_guest_state_area.tr.segment_limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.segment_limit);
		case 0x4810: PRINT(vmcs_guest_state_area.gdtr.limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gdtr.limit);
		case 0x4812: PRINT(vmcs_guest_state_area.idtr.limit); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.idtr.limit);
		case 0x4814: PRINT(vmcs_guest_state_area.es.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.access_rights);
		case 0x4816: PRINT(vmcs_guest_state_area.cs.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.access_rights);
		case 0x4818: PRINT(vmcs_guest_state_area.ss.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.access_rights);
		case 0x481A: PRINT(vmcs_guest_state_area.ds.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.access_rights);
		case 0x481C: PRINT(vmcs_guest_state_area.fs.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.access_rights);
		case 0x481E: PRINT(vmcs_guest_state_area.gs.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.access_rights);
		case 0x4820: PRINT(vmcs_guest_state_area.ldtr.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.access_rights);
		case 0x4822: PRINT(vmcs_guest_state_area.tr.access_rights); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.access_rights);
		case 0x4824: PRINT(vmcs_guest_state_area.interruptibility_state); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.interruptibility_state);
		case 0x4826: PRINT(vmcs_guest_state_area.activity_state); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.activity_state);
		case 0x4828: PRINT(vmcs_guest_state_area.smbase); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.smbase);
		case 0x482A: PRINT(vmcs_guest_state_area.msr_sysenter_cs); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_sysenter_cs);
		case 0x482E: PRINT(vmcs_guest_state_area.vmx_preemption_timer); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.vmx_preemption_timer);

		case 0x4C00: PRINT(vmcs_host_state_area.msr_sysenter_cs); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_sysenter_cs);

		default:
			break;
	}

	return -1;
}

static int32_t get_vmcs_offset_target(target_ulong vmcs_field_encoding, int32_t is_write){

	switch ((vmcs_field_encoding >> 1) << 1){
		case 0x6000: PRINT(vmcs_vmexecution_control_fields.mask_cr0); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.mask_cr0); 
		case 0x6002: PRINT(vmcs_vmexecution_control_fields.mask_cr4); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.mask_cr4);
		case 0x6004: PRINT(vmcs_vmexecution_control_fields.read_shadow_cr0); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_shadow_cr0);
		case 0x6006: PRINT(vmcs_vmexecution_control_fields.read_shadow_cr4); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.read_shadow_cr4);
		case 0x6008: PRINT(vmcs_vmexecution_control_fields.cr3_target[0]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[0]);
		case 0x600A: PRINT(vmcs_vmexecution_control_fields.cr3_target[1]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[1]);
		case 0x600C: PRINT(vmcs_vmexecution_control_fields.cr3_target[2]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[2]);
		case 0x600E: PRINT(vmcs_vmexecution_control_fields.cr3_target[3]); return offsetof(vtx_vmcs_t, vmcs_vmexecution_control_fields.cr3_target[3]);

		case 0x6400: 
			if (!is_write) {
			 	PRINT(vmcs_vmexit_information_fields.exit_qualification); 
				return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.exit_qualification); 
			} else {
				return -2;
			}
		case 0x6402:
			if (!is_write) {
			 	PRINT(vmcs_vmexit_information_fields.io_rcx); 
				return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rcx); 
			} else {
				return -2;
			}
		case 0x6404:
			if (!is_write) {
			 PRINT(vmcs_vmexit_information_fields.io_rsi); 
			 return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rsi); 
			}
			else {
				return -2;
			}
		case 0x6406:
			if (!is_write) {
			 return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rdi); 
			}
			else {
				return -2;
			}
		case 0x6408:
			if (!is_write) {
			 return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.io_rip); 
			}
			else {
				return -2;
			}
		case 0x640A: 
			if (!is_write) {
			     return offsetof(vtx_vmcs_t, vmcs_vmexit_information_fields.guest_linear_addr); 
			}
			else {
				return -2;
			}

		case 0x6800: PRINT(vmcs_guest_state_area.cr0); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr0);
		case 0x6802: PRINT(vmcs_guest_state_area.cr3); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr3);
		case 0x6804: PRINT(vmcs_guest_state_area.cr4); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cr4);
		case 0x6806: PRINT(vmcs_guest_state_area.es.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.es.base_addr);
		case 0x6808: PRINT(vmcs_guest_state_area.cs.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.cs.base_addr);
		case 0x680A: PRINT(vmcs_guest_state_area.ss.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ss.base_addr);
		case 0x680C: PRINT(vmcs_guest_state_area.ds.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ds.base_addr);
		case 0x680E: PRINT(vmcs_guest_state_area.fs.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.fs.base_addr);
		case 0x6810: PRINT(vmcs_guest_state_area.gs.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gs.base_addr);
		case 0x6812: PRINT(vmcs_guest_state_area.ldtr.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.ldtr.base_addr);
		case 0x6814: PRINT(vmcs_guest_state_area.tr.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.tr.base_addr);
		case 0x6816: PRINT(vmcs_guest_state_area.gdtr.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.gdtr.base_addr);
		case 0x6818: PRINT(vmcs_guest_state_area.idtr.base_addr); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.idtr.base_addr);
		case 0x681A: PRINT(vmcs_guest_state_area.dr7); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.dr7);
		case 0x681C: PRINT(vmcs_guest_state_area.esp); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.esp);
		case 0x681E: PRINT(vmcs_guest_state_area.eip); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.eip);
		case 0x6820: PRINT(vmcs_guest_state_area.eflags); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.eflags);
		case 0x6822: PRINT(vmcs_guest_state_area.pending_debug_exceptions); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.pending_debug_exceptions);
		case 0x6824: PRINT(vmcs_guest_state_area.msr_ia32_sysenter_esp); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_sysenter_esp);
		case 0x6826: PRINT(vmcs_guest_state_area.msr_ia32_sysenter_eip); return offsetof(vtx_vmcs_t, vmcs_guest_state_area.msr_ia32_sysenter_eip);

		case 0x6C00: PRINT(vmcs_host_state_area.cr0); return offsetof(vtx_vmcs_t, vmcs_host_state_area.cr0);
		case 0x6C02: PRINT(vmcs_host_state_area.cr3); return offsetof(vtx_vmcs_t, vmcs_host_state_area.cr3);
		case 0x6C04: PRINT(vmcs_host_state_area.cr4); return offsetof(vtx_vmcs_t, vmcs_host_state_area.cr4);
		case 0x6C06: PRINT(vmcs_host_state_area.fs_base_addr); return offsetof(vtx_vmcs_t, vmcs_host_state_area.fs_base_addr);
		case 0x6C08: PRINT(vmcs_host_state_area.gs_base_addr); return offsetof(vtx_vmcs_t, vmcs_host_state_area.gs_base_addr);
		case 0x6C0A: PRINT(vmcs_host_state_area.tr_base_addr); return offsetof(vtx_vmcs_t, vmcs_host_state_area.tr_base_addr);
		case 0x6C0C: PRINT(vmcs_host_state_area.gdtr_base_addr); return offsetof(vtx_vmcs_t, vmcs_host_state_area.gdtr_base_addr);
		case 0x6C0E: PRINT(vmcs_host_state_area.idtr_base_addr); return offsetof(vtx_vmcs_t, vmcs_host_state_area.idtr_base_addr);
		case 0x6C10: PRINT(vmcs_host_state_area.msr_ia32_sysenter_esp); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_sysenter_esp);
		case 0x6C12: PRINT(vmcs_host_state_area.msr_ia32_sysenter_eip); return offsetof(vtx_vmcs_t, vmcs_host_state_area.msr_ia32_sysenter_eip);
		case 0x6C14: PRINT(vmcs_host_state_area.esp); return offsetof(vtx_vmcs_t, vmcs_host_state_area.esp);
		case 0x6C16: PRINT(vmcs_host_state_area.eip); return offsetof(vtx_vmcs_t, vmcs_host_state_area.eip);

		default: break;
	}

	return -1;
}


void helper_vtx_vmread(CPUX86State * env, target_ulong dest, target_ulong vmcs_field_encoding){

	LOG_ENTRY;


	CPUState *cs = CPU(x86_env_get_cpu(env));
	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION /* AND shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		LOG("GEN PROTECTION FAULT");
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs_ptr_register == VMCS_CLEAR_ADDRESS) or
			   (env->vmx_operation == VMX_NON_ROOT_OPERATION /*and  vmcs link ptr */)){
		LOG("FAIL_INVALID");
		vm_exception(FAIL_INVALID, 0, env);
	} else {

		int32_t offset;

		if (env->vmx_operation == VMX_ROOT_OPERATION){

			switch ((vmcs_field_encoding >> 13) & 0x3){
				case 0:	/* 16 bit field */
					offset = get_vmcs_offset16(vmcs_field_encoding, 0);
					if (offset == -1){
						LOG("offset = -1");
						printf("for vmcs_field_encoding = %d\n", vmcs_field_encoding);
						env->vmread_output.vmcs_encoding = offset;
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						env->vmread_output.vmcs_encoding = offset;
					} else {
						LOG("VMREAD working...");
						env->vmread_output.vmread_field = x86_lduw_phys(cs, env->vmcs_ptr_register + offset);
						printf("Got value %d\n", env->vmread_output.vmread_field);
						env->vmread_output.vmcs_encoding = vmcs_field_encoding;
						vm_exception(SUCCEED,0, env);
					}
					break;
				case 1: /* 64 bit field */
					offset = get_vmcs_offset64(vmcs_field_encoding, 0);
					if (offset == -1){
						LOG("offset = -1");
						printf("for vmcs_field_encoding = %d\n", vmcs_field_encoding);
						env->vmread_output.vmcs_encoding = offset;
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						env->vmread_output.vmcs_encoding = offset;
					} else {
						LOG("VMREAD working...");
						if (vmcs_field_encoding & 0x02)
							env->vmread_output.vmread_field = x86_ldl_phys(cs, env->vmcs_ptr_register + offset);
						else
							env->vmread_output.vmread_field = x86_ldq_phys(cs, env->vmcs_ptr_register + offset);
						
						printf("Got value %d\n", env->vmread_output.vmread_field);
						env->vmread_output.vmcs_encoding = vmcs_field_encoding;
						vm_exception(SUCCEED,0, env);
					}
					
					break;
				case 2: /* 32 bit field */
					offset = get_vmcs_offset32(vmcs_field_encoding, 0);
					if (offset == -1){
						LOG("offset = -1");
						printf("for vmcs_field_encoding = %d\n", vmcs_field_encoding);
						env->vmread_output.vmcs_encoding = offset;
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						env->vmread_output.vmcs_encoding = offset;
					} else {
						LOG("VMREAD working...");
						env->vmread_output.vmread_field = x86_ldl_phys(cs, env->vmcs_ptr_register + offset);
						printf("Got value %d\n", env->vmread_output.vmread_field);
						env->vmread_output.vmcs_encoding = vmcs_field_encoding;
						vm_exception(SUCCEED,0, env);
					}
					break;
				case 3: /* target ulong width field */
					offset = get_vmcs_offset_target(vmcs_field_encoding, 0);
					if (offset == -1){
						LOG("offset = -1");
						printf("for vmcs_field_encoding = %d\n", vmcs_field_encoding);
						env->vmread_output.vmcs_encoding = offset;
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						env->vmread_output.vmcs_encoding = offset;
					} else {
						LOG("VMREAD working...");
						env->vmread_output.vmread_field = x86_ldl_phys(cs, env->vmcs_ptr_register + offset);
						printf("Got value %d\n", env->vmread_output.vmread_field);
						env->vmread_output.vmcs_encoding = vmcs_field_encoding;
						vm_exception(SUCCEED,0, env);
					}
					break;
				default:
					LOG("NO MATCH");
					break;
			}

		} else {
			LOG("unsupported VMCS Component");
			vm_exception(FAIL, 12, env);
		}

	} 
	// else if (src op no correpsonse){
	// 	LOG("unsupported VMCS Component");
	// 	vm_exception(FAIL, 12, env);
	// } else {
	// 	// if (env->vmx_operation == VMX_ROOT_OPERATION){
	// 	// 	*dest = 
	// 	// } else {
	// 	// 	*dest = 
	// 	// }
	// 	// vm_exception(SUCCEED,0, env);
	// }
	LOG_EXIT;
}



void helper_vtx_vmwrite(CPUX86State * env, target_ulong vmcs_field_encoding, target_ulong data){

	LOG_ENTRY;


	CPUState *cs = CPU(x86_env_get_cpu(env));
	int cpl = env->segs[R_CS].selector & 0x3;

	/* again, check DESC_L_MASK 64 bit only ? */
	if (env->vmx_operation == VMX_DISABLED or
		!ISSET(env->cr[0], CR0_PE_MASK) or
		ISSET(env->eflags, VM_MASK) or 
		(ISSET(env->efer, MSR_EFER_LMA) and !ISSET(env->segs[R_CS].flags, DESC_L_MASK))){
			raise_exception(env, EXCP06_ILLOP);
	} else if (env->vmx_operation == VMX_NON_ROOT_OPERATION /*AND  shadow stuff */){
		/* vm exit */
	} else if (cpl > 0){
		raise_exception(env, EXCP0D_GPF);
	} else if ((env->vmx_operation == VMX_ROOT_OPERATION and env->vmcs_ptr_register == VMCS_CLEAR_ADDRESS)){
			   //(env->vmx_operation == VMX_NON_ROOT_OPERATION and /* vmcs link ptr */)){
		vm_exception(FAIL_INVALID, 0, env);
	} else {

		int32_t offset;

		if (env->vmx_operation == VMX_ROOT_OPERATION){
			switch ((vmcs_field_encoding >> 13) & 0x3){
				case 0:	/* 16 bit field */
					offset = get_vmcs_offset16(vmcs_field_encoding, 1);
					if (offset == -1){
						LOG("offset = -1");
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						
					} else {
						LOG("VMWRITE working...");
						x86_stw_phys(cs, env->vmcs_ptr_register + offset, data);
						memcpy((uint16_t *) ((unsigned char *)(env->processor_vmcs) + offset), &data, sizeof(uint16_t));
						vm_exception(SUCCEED,0, env);
						
					}
					break;
				case 1: /* 64 bit field */
					offset = get_vmcs_offset64(vmcs_field_encoding, 1);
					if (offset == -1){
						LOG("offset = -1");
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						
					} else {
						LOG("VMWRITE working...");
						if (vmcs_field_encoding & 0x02){
							x86_stl_phys(cs, env->vmcs_ptr_register + offset + (sizeof(uint64_t)/2), data);
							memcpy((uint32_t *) ((unsigned char *)(env->processor_vmcs) + offset) + (sizeof(uint64_t)/2), &data, sizeof(uint32_t));
						} else {
							x86_stq_phys(cs, env->vmcs_ptr_register + offset, data);
							memcpy((uint64_t *) ((unsigned char *)(env->processor_vmcs) + offset), &data, sizeof(uint64_t));
						}
						
						//update_processor_vmcs(env);
						vm_exception(SUCCEED,0, env);
						
					}
					
					break;
				case 2: /* 32 bit field */
					offset = get_vmcs_offset32(vmcs_field_encoding, 1);
					if (offset == -1){
						LOG("offset = -1");
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						
					} else {
						LOG("VMWRITE working...");
						x86_stl_phys(cs, env->vmcs_ptr_register + offset, data);
						//update_processor_vmcs(env);
						memcpy((uint32_t *) ((unsigned char *)(env->processor_vmcs) + offset), &data, sizeof(uint32_t));
						vm_exception(SUCCEED,0, env);
						
					}
					break;
				case 3: /* target ulong width field */
					offset = get_vmcs_offset_target(vmcs_field_encoding, 1);
					if (offset == -1){
						LOG("offset = -1");
					} else if (offset == -2){
						vm_exception(FAIL, 13, env);
						LOG("offset = -2");
						
					} else {
						LOG("VMWRITE working...");
						x86_stl_phys(cs, env->vmcs_ptr_register + offset, data);
						//update_processor_vmcs(env);
						memcpy((uint32_t *) ((unsigned char *)(env->processor_vmcs) + offset), &data, sizeof(uint32_t));
						vm_exception(SUCCEED,0, env);
						
					}
					break;
				default:
					LOG("NO MATCH");
					break;
			}
		} else {
			vm_exception(FAIL, 12, env);
		}

	}

	// note: some execution paths do not end up here
	LOG_EXIT;
}

