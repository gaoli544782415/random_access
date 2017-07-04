/*
******************************************************************************
Vendor              : BGI 
Associated Filename : kernel.cl
Purpose             : OpenCL C example checking kernel to glboal memory bandwidth
Revision History    : 2017.06.29
Designer            :Gao Li
******************************************************************************
*/
__kernel 
__attribute__ ((reqd_work_group_size(1,1,1)))
void bandwidth(
               __global uint16  * __restrict input0     , 
               __global uint16  * __restrict output0    ,               
#ifdef USE_4DDR
               __global uint16  * __restrict input1     , 
               __global uint16  * __restrict output1    ,
#endif
               ulong num_blocks
               )
{

    ulong       blockindex       ;
    ulong       rand_addr_reg    ;
    ulong       rand_addr        ;
    ulong       num_blocks_2     ;

    uint16      temp0            ;
    uint16      temp1            ;  
     
    blockindex = 0               ;
    num_blocks_2 =  10000        ;
    __attribute__((xcl_pipeline_loop))
    for (blockindex=0; blockindex<num_blocks_2; blockindex++)
    {
    	  rand_addr_reg = (blockindex*101 + 12345)%32768          ;

    	  if((rand_addr_reg<num_blocks)&&(rand_addr_reg>0))
    	  {
    		  rand_addr = rand_addr_reg          ;
    	   }
    	  else
    	  {
    		  rand_addr = 0                      ;
    	  }

          temp0 = input0[rand_addr]           ;
          output0[rand_addr] = temp0          ;

          #ifdef USE_4DDR
              temp1 = input1[rand_addr]       ;
              output1[rand_addr] = temp1      ;
          #endif        
    }
}


