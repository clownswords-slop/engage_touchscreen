// ===== inline hooks =====
// The game function's first instruction becomes `b handler`; the handler calls the original through a trampoline in our own
// .text: [displaced instruction][ldr x16, #8][br x16][.quad target+4]. Code is written with code_write (patch.h).
asm(".section .text.tramp,\"ax\"\n.balign 16\n.global g_tramp\n.hidden g_tramp\ng_tramp: .space 0x600\n.text");
extern u32 g_tramp[];
static int g_ntramp;
static u32 g_hook_err;
static int a64_pcrel(u32 i){ return (i&0x1f000000u)==0x10000000u || (i&0x7c000000u)==0x14000000u || (i&0xff000010u)==0x54000000u ||
  (i&0x7e000000u)==0x34000000u || (i&0x7e000000u)==0x36000000u || (i&0x3b000000u)==0x18000000u; }
// returns the trampoline (call it as the original), 0 on failure (g_hook_err says why)
static u64 hook_install(u64 target,u64 handler){
  if(g_ntramp>=0x600/24){ g_hook_err=1; return 0; }
  u32 ins=RD32(target); if(a64_pcrel(ins)){ g_hook_err=2; return 0; }
  if(!a64_in_range(target,handler)){ g_hook_err=3; return 0; }
  u64 t=(u64)&g_tramp[g_ntramp*6]; u64 back=target+4;
  u32 tb[6]={ ins, 0x58000050u, 0xd61f0200u, (u32)back, (u32)(back>>32), 0xd503201fu };
  if(!code_write(t,tb,24)){ g_hook_err=0x100|g_patch_rc; return 0; }
  g_ntramp++;
  if(!code_write32(target,a64_branch(target,handler,0))){ g_hook_err=0x200|g_patch_rc; return 0; }
  return t; }
