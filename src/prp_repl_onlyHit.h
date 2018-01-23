#ifndef PRP_REPL_H_
#define PRP_REPL_H_

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "repl_policies.h"
// the number of asscociated ways
#define PRP_WAY_VALUE 16
//P_hit(t): 6bins value [bin0 bin1...bin5]
#define PRP_PTable0 15
#define PRP_PTable1 14
#define PRP_PTable2 12
#define PRP_PTable3 10
#define PRP_PTable4 9
#define PRP_PTable5 1

// OUTPUT_LOG = 1, dump N_status to dump_test at the result time
// INPUT_LOG = 1, load dump_test at initial time
// Only one of them is 1 at one time, default is OUTPUT_LOG = 0, INPUT_LOG = 0
#define OUTPUT_LOG    0
#define INPUT_LOG     1

//the result access time for each pattern dump "dramBinArray"
//for b_zip. test for short time
//#define SAVE_COUNT 1950000

//for gcc
#define SAVE_COUNT 1740000

//for xalan
//#define SAVE_COUNT 430000

//for cactus
//#define SAVE_COUNT 510000

//for leslie3d
//#define SAVE_COUNT 2630000

//for mcf
//#define SAVE_COUNT 10190000

//for soplex
//#define SAVE_COUNT 3310000


// PRP
class PRPReplPolicy : public ReplPolicy {
    protected:
        // add class member variables here	
		uint32_t*   set_TS;    //one timestamp for each set, unit:0-1023, M in paper
		uint32_t    PTable[6]; //15-14-12-10-9-1, P_hit(t) in paper
		uint64_t*   idArray;   //each line to which mem, to record Virtual line Address(lineAddr), VA[63:6]
		// for dramBinArray and dramTsArray, first index: VA[31:12] which is page index. assume page size is 4KB = VA[11:0].
		uint32_t**  dramBinArray;  //each element with 6 bins for 6 intervals, each elemnet is N_L(i) in paper.
		uint32_t**  dramTsArray;   //each element with 64 timeStamps for 64 cache blocks, 4KB Page includes 64 cache blocks(one block is 64 Byte) 
		uint32_t    numLines;      //the number of cache lines in LLC
		uint64_t    new_insert_lineAddr;   // to temp record the line address of new inserted cache block
		uint32_t    log_counter;           // to global record how many times is cache accessed. For dump/load data counter.
		bool        cache_miss_flag;      //represent cache-miss case,  flag = true when replace, and clear this flag after each update.
		bool*       usedLineArray;        //Each element becomes true when this cache is used firstly
		
		//function for normalizing overflow 4-bit counter, which is applied on 6 bins of one dramBinArray[pageAddr].
		void half_value_arrayN(uint32_t pageAddr){
			(dramBinArray[pageAddr][0])/=2;
			(dramBinArray[pageAddr][1])/=2;
			(dramBinArray[pageAddr][2])/=2;
			(dramBinArray[pageAddr][3])/=2;
			(dramBinArray[pageAddr][4])/=2;
			(dramBinArray[pageAddr][5])/=2;
		}

    public:
        // add member methods here, refer to repl_policies.h
		explicit PRPReplPolicy(uint32_t _numLines) : numLines(_numLines) {
			dramBinArray = gm_calloc<uint32_t*>(1024*1024);  // VA[31:12]=20bits, there is 2^20 pages.
			dramTsArray = gm_calloc<uint32_t*>(1024*1024);   // VA[63:32] is truncated.
			for(uint32_t i = 0; i < (1024*1024); i++){
				dramBinArray[i] = gm_calloc<uint32_t>(6);    // 6 bins for each page
				dramTsArray[i] = gm_calloc<uint32_t>(64);    // each page includes 64 cache blocks
				for(uint32_t j = 0; j < 6; j++)
					dramBinArray[i][j] = 0;
				for(uint32_t j = 0; j < 64; j++)
					dramTsArray[i][j] = 0;
			}
			set_TS =  gm_calloc<uint32_t>(numLines/PRP_WAY_VALUE); // the number of sets is lines/16 way-associated
			idArray = gm_calloc<uint64_t>(numLines);
			usedLineArray = gm_calloc<bool>(numLines);
			for(uint32_t i = 0; i < numLines; i++){
				idArray[i] = 0;
				usedLineArray[i] = false;
			}
			for(uint32_t i = 0; i < numLines/PRP_WAY_VALUE; i++){
				set_TS[i] = 0;
			}
			PTable[0] = PRP_PTable0;
			PTable[1] = PRP_PTable1;
			PTable[2] = PRP_PTable2;
			PTable[3] = PRP_PTable3;
			PTable[4] = PRP_PTable4;
			PTable[5] = PRP_PTable5;	
			new_insert_lineAddr = 0;
			log_counter = 0;
			cache_miss_flag = false;
        }
		~PRPReplPolicy() {
			for(uint32_t i = 0; i < (1024*1024); i++){
				gm_free(dramBinArray[i]);
				gm_free(dramTsArray[i]);
			}
			gm_free(dramBinArray);
			gm_free(dramTsArray);
			gm_free(idArray);
			gm_free(usedLineArray);
			gm_free(set_TS);
        }
		
		void update(uint32_t id, const MemReq* req) {
			uint64_t reqAddr = ((req->lineAddr)<<6)&0x00000000FFFFFFFF;  //req->lineAdd = VA[63:5], reqAddr = VA[31:0] align to line
			uint32_t pageAddr = (reqAddr>>12); //pageAddr = VA[31:12]
			uint32_t line_index = ((uint32_t)(req->lineAddr))&0x0000003F; //line_index = VA[11:6], one page with 64 cache blocks
#if INPUT_LOG
            if(log_counter==0){  //load N_L(i) into dramBinArray from the file called dump_test
				string fileName = "dump_test";
				fstream fp(fileName, ios::in);
				if (!fp) {
					cout << "Fail to open file: " << fileName << endl;
				}
				string tmpLine;
				uint32_t log_page_num = 0;
				uint32_t N_0, N_1, N_2, N_3, N_4, N_5;
				while (getline(fp, tmpLine)) {
					if(log_page_num < 1024*1024){  //protect for the unexpected last endl or illegal input
						stringstream ssin(tmpLine);
						ssin >> N_0 >> N_1 >> N_2 >> N_3 >> N_4 >> N_5;
						dramBinArray[log_page_num][0] = N_0;
						dramBinArray[log_page_num][1] = N_1;
						dramBinArray[log_page_num][2] = N_2;
						dramBinArray[log_page_num][3] = N_3;
						dramBinArray[log_page_num][4] = N_4;
						dramBinArray[log_page_num][5] = N_5;
					}
					log_page_num++;
				}
				cout << "load source done" << endl;
				fp.close();					
			}
#endif	
			uint32_t set_index = id/PRP_WAY_VALUE; 
			set_TS[set_index]+=1;    //update the specific set's TimeStamp
			if(set_TS[set_index] >= 1024){
		        set_TS[set_index] = 0; //0-1023, 10 bit accuracy
			}
			if(cache_miss_flag == false){  //Only hit case, update reused frequency to the corresponding Bin_array
				uint32_t set_t =set_TS[set_index];
				uint32_t line_t = dramTsArray[pageAddr][line_index];
				uint32_t diff_t = 0;
				//diff_t <-- calc re-ref time period this time = set_TS - line_TS:
				if(line_t > set_t){
					diff_t = (1024 + set_t) - line_t;  //wrap around case, becasue set TS is overflow back to 0.
				}
				else{
					diff_t = set_t - line_t;
				}
				
				//update NL
				if(diff_t < 16){
					dramBinArray[pageAddr][0]+=1;    //re-ref time: 1-15
					if(dramBinArray[pageAddr][0] == 16)
						half_value_arrayN(pageAddr);
				}
				else if(diff_t < 32){
					dramBinArray[pageAddr][1]+=1;    //re-ref time: 16-31
					if(dramBinArray[pageAddr][1] == 16)
						half_value_arrayN(pageAddr);
				}
				else if(diff_t < 64){
					dramBinArray[pageAddr][2]+=1;    //re-ref time: 32-63
					if(dramBinArray[pageAddr][2] == 16)
						half_value_arrayN(pageAddr);
				}
				else if(diff_t < 128){
					dramBinArray[pageAddr][3]+=1;    //re-ref time: 64-127
					if(dramBinArray[pageAddr][3] == 16)
						half_value_arrayN(pageAddr);
				}
				else if(diff_t < 256){
					dramBinArray[pageAddr][4]+=1;    //re-ref time: 128-255
					if(dramBinArray[pageAddr][4] == 16)
						half_value_arrayN(pageAddr);
				}
				else{
					dramBinArray[pageAddr][5]+=1;    //re-ref time: 256-inf
					if(dramBinArray[pageAddr][5] == 16)
						half_value_arrayN(pageAddr);
				}
			}
			dramTsArray[pageAddr][line_index] = set_TS[set_index];  //update line TS <-- set_TS
            log_counter++;
			cache_miss_flag = false;   //clear possible miss status after update function.
#if OUTPUT_LOG
            if(log_counter == SAVE_COUNT){  //dump N_L(i) in dramBinArray to the file called dump_test
				string fileName = "dump_test";
			    fstream fp;
				fp.open(fileName, ios::out);
				if (!fp) {
		            cout << "Fail to open file: " << fileName << endl;
	            }
				for(uint32_t t_i = 0; t_i < (1024*1024); t_i++){
					for(uint32_t t_j = 0; t_j < 6; t_j++){
						fp << dramBinArray[t_i][t_j];
						if(t_j < 5)
							fp << " ";
						else
							fp << endl; //next line for next page
					}
				}
				cout << "log done" << endl;
				fp.close();	
			}
#endif			
        }
		
		void replaced(uint32_t id) {
			idArray[id] = new_insert_lineAddr;  //record access line-addr to idArray[id]
			usedLineArray[id] = true;  //This cache is surely used firstly.
            cache_miss_flag = true;	   // This is cache miss case.	
			return;
        }

		template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
            uint32_t bestCand = 0;  //return cache id
			uint32_t test_array[PRP_WAY_VALUE];	 //to save calculated P_L^hit of these candidates in one set  
			auto si = cands.begin();  //the start line id of this set
		    uint32_t set_index = (*si)/PRP_WAY_VALUE;
			uint32_t start_line_id = (*si);
			uint32_t line_id = 0;	
			
			//calc every candidate's P_L^hit value, in each line in this set
			for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
				line_id = *ci;			
				if(usedLineArray[line_id]==false){ //fast decision, when there is no-used cache line.
					new_insert_lineAddr = req->lineAddr;  //record access line-addr to cache idArray[id]
					bestCand = line_id;
                    return bestCand;
				}
				uint32_t set_t =  set_TS[set_index];  //Set_TS
				uint64_t savedLineAddr = idArray[line_id]; // VA[63:6]
				uint64_t reqAddr = ((savedLineAddr)<<6)&0x00000000FFFFFFFF;  //VA:[31:0] align line
			    uint32_t pageAddr = (reqAddr>>12); //VA[31:12]
			    uint32_t line_index = ((uint32_t)(savedLineAddr))&0x0000003F; //VA[11:6], there 64 lines in one page 
				
				uint32_t line_t = dramTsArray[pageAddr][line_index]; //Line_TS
				uint32_t diff_t = 0;
				//calc re-ref time period this time:
				if(line_t > set_t){
					diff_t = (1024 + set_t) - line_t;
				}
				else{
					diff_t = set_t - line_t;
				}
				
				//calc P_L^hit in 6 accumulated terms
				//calc. progress: with smaller re-ref time t, the more terms need to add.
				uint32_t sum_NL_P = 0; // numerator of Probability calculator unit...eq5 in paper
				uint32_t sum_NL =0; // denominator of Probability calculator unit...eq5 in paper
				sum_NL_P += (dramBinArray[pageAddr][5]*PTable[5]);
				sum_NL += dramBinArray[pageAddr][5];
				if(diff_t < 256){
					sum_NL_P += (dramBinArray[pageAddr][4]*PTable[4]);
				    sum_NL += dramBinArray[pageAddr][4];
				}
				if(diff_t < 128){
					sum_NL_P += (dramBinArray[pageAddr][3]*PTable[3]);
				    sum_NL += dramBinArray[pageAddr][3];
				}
				if(diff_t < 64){
					sum_NL_P += (dramBinArray[pageAddr][2]*PTable[2]);
				    sum_NL += dramBinArray[pageAddr][2];
				}
				if(diff_t < 32){
					sum_NL_P += (dramBinArray[pageAddr][1]*PTable[1]);
				    sum_NL += dramBinArray[pageAddr][1];
				}
				if(diff_t < 16){
					sum_NL_P += ( dramBinArray[pageAddr][0]*PTable[0]);
				    sum_NL +=  dramBinArray[pageAddr][0];
				}
				if(sum_NL != 0)  //protect denominator == 0
				    test_array[line_id - start_line_id] = sum_NL_P/sum_NL;
				else
					test_array[line_id - start_line_id] = 0;
			}
			
			// choose the smallest P_L^hit value in test_array, has lowest hit P in the future
			uint32_t min_value = 1536; // 16*16*6/1 = 1536, P_L^hit value can't larger than this value
			bestCand = 0;  //if all candidates have the same value, return the left one
			for(uint32_t i = 0; i < PRP_WAY_VALUE; i++){
				if(test_array[i] < min_value){
				    bestCand = i;  //0-15, offset index in this set
					min_value = test_array[i];
				}
			}
			bestCand += start_line_id; //return to line_addr
			new_insert_lineAddr = req->lineAddr;   //record access line-addr to cache idArray[id]
            return bestCand;
        }
        DECL_RANK_BINDINGS;
};
#endif // PRP_REPL_H_
