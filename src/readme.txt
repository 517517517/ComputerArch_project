In prp_repl.h:
With #define OUTPUT_LOG 0 and #define INPUT_LOG 0 setting: PRP_V1.

With #define OUTPUT_LOG 1 and #define INPUT_LOG 0 setting: PRP_V2.
According to patterns to adjust SAVE_COUNT, and Execute it. the dump_test (N_L) will generate in the folder: casim/zsim/

With #define OUTPUT_LOG 0 and #define INPUT_LOG 1 setting: PRP_V2.
PRP loads the dump_test (N_L) from  casim/zsim/ before accessing any LLC information.

prp_repl_onlyHit.h: almost the same as PRP_V2, but it only update N_L information when cache hit: PRP_V3.

PRP_V2 performs well than PRP_V3, so prp_repl.h is main work I would like to submit.
For running PRP_V1 or PRP_V2, just put init.cpp and prp_repl.h into zsim/src/, and also add the configs to config folder.
The default defined setting in prp_repl.h is PRP_V1, now.
Command Example: ./hw4runscript SPEC gcc PRP


