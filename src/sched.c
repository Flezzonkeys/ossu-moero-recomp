#include "rt.h"
s16 audio_buf[2*800];int audio_n;
void run_frame(void){static double acc;acc+=32768.0/59.8261;audio_n=(int)acc;acc-=audio_n;ppu_frame_begin();int done=0;for(int l=0;l<263;l++){io_frame_line(l);if(l<192)ppu_render_line(l);if(l==192)gx_vblank();
 for(int s=0;s<8;s++){cpu_run(&cpu9,1065/2);cpu_run(&cpu7,266/2);}
 timers_tick(&cpu9,2130);timers_tick(&cpu7,2130);
 int want=audio_n*(l+1)/263;if(want>done){spu_mix(audio_buf+done*2,want-done);done=want;}}}
