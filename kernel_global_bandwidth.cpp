/*******************************************************************************
Vendor: Xilinx 
Associated Filename: kernel_global_bandwidth.c
Purpose: OpenCL host code checking kernel to glboal memory bandwidth
Revision History: 
Feb 13, 2016: initial release
Feb 25, 2016: Modified to support TUL KU115 2DDR DSA
May 20, 2016: Modified to support TUL KU115 4DDR DSA
                                                
*******************************************************************************
Copyright (C) 2016 XILINX, Inc.

This file contains confidential and proprietary information of Xilinx, Inc. and 
is protected under U.S. and international copyright and other intellectual 
property laws.

DISCLAIMER
This disclaimer is not a license and does not grant any rights to the materials 
distributed herewith. Except as otherwise provided in a valid license issued to 
you by Xilinx, and to the maximum extent permitted by applicable law: 
(1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS, AND XILINX 
HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, 
INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR 
FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether 
in contract or tort, including negligence, or under any other theory of 
liability) for any loss or damage of any kind or nature related to, arising under 
or in connection with these materials, including for any direct, or any indirect, 
special, incidental, or consequential loss or damage (including loss of data, 
profits, goodwill, or any type of loss or damage suffered as a result of any 
action brought by a third party) even if such damage or loss was reasonably 
foreseeable or Xilinx had been advised of the possibility of the same.

CRITICAL APPLICATIONS
Xilinx products are not designed or intended to be fail-safe, or for use in any 
application requiring fail-safe performance, such as life-support or safety 
devices or systems, Class III medical devices, nuclear facilities, applications 
related to the deployment of airbags, or any other applications that could lead 
to death, personal injury, or severe property or environmental damage 
(individually and collectively, "Critical Applications"). Customer assumes the 
sole risk and liability of any use of Xilinx products in Critical Applications, 
subject only to applicable laws and regulations governing limitations on product 
liability. 

THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT 
ALL TIMES.

*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <CL/opencl.h>
#include <CL/cl_ext.h>

/////////////////////////////////////////////////////////////////////////////////
//load_file_to_memory
//Allocated memory for and load file from disk memory
//Return value 
// 0   Success
//-1   Failure to open file
//-2   Failure to allocate memory


//#define USE_4DDR


int load_file_to_memory(const char *filename, char **result,size_t *inputsize)
{ 
    int size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) { 
        *result = NULL;
        return -1; // -1 means file opening fail 
    } 
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);
    if (size != fread(*result, sizeof(char), size, f)) 
        { 
            free(*result);
            return -2; // -2 means file reading fail 
        } 
    fclose(f);
    (*result)[size] = 0;
    if(inputsize!=NULL) (*inputsize)=size;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////
//opencl_setup
//Create context for Xilinx platform, Accelerator device
//Create single command queue for accelerator device
//Create program object with clCreateProgramWithBinary using given xclbin file name
//Return value
// 0    Success
//-1    Error
//-2    Failed to load XCLBIN file from disk
//-3    Failed to clCreateProgramWithBinary
int opencl_setup(const char *xclbinfilename, cl_platform_id *platform_id, 
                 cl_device_id *devices, cl_device_id *device_id, cl_context  *context, 
                 cl_command_queue *command_queue, cl_program *program, 
                 char *cl_platform_name, const char *target_device_name) {

    char cl_platform_vendor[1001];
    char cl_device_name[1001];
    cl_int err;
    cl_uint num_devices;
    unsigned int device_found = 0;

    // Get first platform
    err = clGetPlatformIDs(1,platform_id,NULL);
    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to find an OpenCL platform!\n");
        printf("ERROR: Test failed\n");
        return -1;
    }
    err = clGetPlatformInfo(*platform_id,CL_PLATFORM_VENDOR,1000,(void *)cl_platform_vendor,NULL);
    if (err != CL_SUCCESS) {
        printf("ERROR: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
        printf("ERROR: Test failed\n");
        return -1;
    }
    printf("CL_PLATFORM_VENDOR %s\n",cl_platform_vendor);
    err = clGetPlatformInfo(*platform_id,CL_PLATFORM_NAME,1000,(void *)cl_platform_name,NULL);
    if (err != CL_SUCCESS) {
            printf("ERROR: clGetPlatformInfo(CL_PLATFORM_NAME) failed!\n");
            printf("ERROR: Test failed\n");
            return -1;
    }
    printf("CL_PLATFORM_NAME %s\n",cl_platform_name);

    // Get Accelerator compute device
    int accelerator = 1;
    err = clGetDeviceIDs(*platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices);
    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to create a device group!\n");
        printf("ERROR: Test failed\n");
        return -1;
    }

    //iterate all devices to select the target device. 
    for (int i=0; i<num_devices; i++) {
        err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 1024, cl_device_name, 0);
        if (err != CL_SUCCESS) {
            printf("Error: Failed to get device name for device %d!\n", i);
            printf("Test failed\n");
            return EXIT_FAILURE;
        }
        //printf("CL_DEVICE_NAME %s\n", cl_device_name);
        if(strcmp(cl_device_name, target_device_name) == 0) {
            *device_id = devices[i];
            device_found = 1;
            printf("Selected %s as the target device\n", cl_device_name);
        }
    }
    
    if (!device_found) {
        printf("Target device %s not found. Exit.\n", target_device_name);
        return EXIT_FAILURE;
    }

    // Create a compute context containing accelerator device
    (*context)= clCreateContext(0, 1, device_id, NULL, NULL, &err);
    if (!(*context))
        {
            printf("ERROR: Failed to create a compute context!\n");
            printf("ERROR: Test failed\n");
            return -1;
        }

    // Create a command queue for accelerator device
    (*command_queue) = clCreateCommandQueue(*context, *device_id, CL_QUEUE_PROFILING_ENABLE, &err);
    if (!(*command_queue))
        {
            printf("ERROR: Failed to create a command commands!\n");
            printf("ERROR: code %i\n",err);
            printf("ERROR: Test failed\n");
            return -1;
        }

    // Load XCLBIN file binary from disk
    int status;
    unsigned char *kernelbinary;
    printf("loading %s\n", xclbinfilename);
    size_t xclbinlength;
    err = load_file_to_memory(xclbinfilename, (char **) &kernelbinary,&xclbinlength);
    if (err != 0) {
        printf("ERROR: failed to load kernel from xclbin: %s\n", xclbinfilename);
        printf("ERROR: Test failed\n");
        return -2;
    }

    // Create the program from XCLBIN file, configuring accelerator device
    (*program) = clCreateProgramWithBinary(*context, 1, device_id, &xclbinlength, (const unsigned char **) &kernelbinary, &status, &err);
    if ((!(*program)) || (err!=CL_SUCCESS)) {
        printf("ERROR: Failed to create compute program from binary %d!\n", err);
        printf("ERROR: Test failed\n");
        return -3;
    }

    // Build the program executable (no-op)
    err = clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
            size_t len;
            char buffer[2048];
            printf("ERROR: Failed to build program executable!\n");
            clGetProgramBuildInfo(*program, *device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
            printf("%s\n", buffer);
            printf("ERROR: Test failed\n");
            return -1;
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////////
//main

int main(int argc, char** argv)
{

#if defined(SDX_PLATFORM) && !defined(TARGET_DEVICE)
  #define STR_VALUE(arg)      #arg
  #define GET_STRING(name) STR_VALUE(name)
  #define TARGET_DEVICE GET_STRING(SDX_PLATFORM)
#endif
    //TARGET_DEVICE macro needs to be passed from gcc command line
    const char *target_device_name = TARGET_DEVICE;

    int err, err1, err2, err3;

    size_t globalbuffersize = 1024*1024*1024;    //1GB

    //Reducing the data size for emulation mode
    char *xcl_mode = getenv("XCL_EMULATION_MODE");
    if (xcl_mode != NULL){
    	globalbuffersize = 1024 * 1024 ;  // 1MB
    }

    //opencl setup
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_device_id devices[16];  // compute device id 
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    char cl_platform_name[1001];

    //variables for profiling
    uint64_t nsduration;
    uint64_t tenseconds = ((uint64_t) 10) * ((uint64_t) 1000000000);

    if (argc != 2){
        printf("Usage: %s <xclbin_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    err = opencl_setup(argv[1], &platform_id, devices, &device_id, 
                       &context, &command_queue, &program, cl_platform_name, 
                       target_device_name);
    if(err==-1){
        printf("Error : general failure setting up opencl context\n");
        return -1;
    }
    if(err==-2) {
        printf("Error : failed to bandwidth.xclbin from disk\n");
        return -1;
    }
    if(err==-3) {
        printf("Error : failed to clCreateProgramWithBinary with contents of xclbin\n");
    }

    //access the ACCELERATOR kernel
    cl_int clstatus;
    cl_kernel kernel = clCreateKernel(program, "bandwidth", &clstatus);
    if (!kernel || clstatus != CL_SUCCESS) {
        printf("Error: Failed to create compute kernel!\n");
        printf("Error: Test failed\n");
        return -1;
    }

    //input buffer
    unsigned char *input_host = ((unsigned char *)malloc(globalbuffersize));
    if(input_host==NULL) {
        printf("Error: Failed to allocate host side copy of OpenCL source buffer of size %i\n",globalbuffersize);
        return -1;
    }
    unsigned int i;
    for(i=0; i<globalbuffersize; i++) 
        input_host[i]=i%256;

    cl_mem input_buffer0, output_buffer0;
#if defined(USE_2DDR) || defined(USE_4DDR) 
    cl_mem_ext_ptr_t input_buffer0_ext, output_buffer0_ext;
    input_buffer0_ext.flags = XCL_MEM_DDR_BANK0;
    input_buffer0_ext.obj = NULL;
    input_buffer0_ext.param = 0;
    input_buffer0 = clCreateBuffer(context, 
                                  CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, 
                                  globalbuffersize, 
                                  &input_buffer0_ext, 
                                  &err);

    output_buffer0_ext.flags = XCL_MEM_DDR_BANK1;
    output_buffer0_ext.obj = NULL;
    output_buffer0_ext.param = 0;
    output_buffer0 = clCreateBuffer(context, 
                                   CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, 
                                   globalbuffersize, 
                                   &output_buffer0_ext, 
                                   &err1);

#if defined(USE_4DDR)
    cl_mem input_buffer1, output_buffer1;
    cl_mem_ext_ptr_t input_buffer1_ext, output_buffer1_ext;
    input_buffer1_ext.flags = XCL_MEM_DDR_BANK2;
    input_buffer1_ext.obj = NULL;
    input_buffer1_ext.param = 0;

    input_buffer1 = clCreateBuffer(context, 
                                  CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, 
                                  globalbuffersize, 
                                  &input_buffer1_ext, 
                                  &err2);

    output_buffer1_ext.flags = XCL_MEM_DDR_BANK3;
    output_buffer1_ext.obj = NULL;
    output_buffer1_ext.param = 0;
    output_buffer1 = clCreateBuffer(context, 
                                   CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, 
                                   globalbuffersize, 
                                   &output_buffer1_ext, 
                                   &err3);
#endif

#else
    input_buffer0 = clCreateBuffer(context, 
                                  CL_MEM_READ_WRITE, 
                                  globalbuffersize, 
                                  NULL, 
                                  &err);

    output_buffer0 = clCreateBuffer(context, 
                                   CL_MEM_READ_WRITE, 
                                   globalbuffersize, 
                                   NULL, 
                                   &err1);
#endif

    if(err != CL_SUCCESS) {
        printf("Error: Failed to allocate input_buffer0 of size %i\n", globalbuffersize);
        return -1;
    }

    if (err1 != CL_SUCCESS) {
        printf("Error: Failed to allocate output_buffer0 of size %i\n", globalbuffersize);
        return -1;
    }

#ifdef USE_4DDR
    if(err2 != CL_SUCCESS) {
        printf("Error: Failed to allocate input_buffer1 of size %i\n", globalbuffersize);
        return -1;
    }

    if (err3 != CL_SUCCESS) {
        printf("Error: Failed to allocate output_buffer1 of size %i\n", globalbuffersize);
        return -1;
    }
#endif

    //
    cl_ulong num_blocks = globalbuffersize/64;
#ifdef USE_4DDR
    double dbytes = globalbuffersize*2.0;
#else
    double dbytes = globalbuffersize;
    //double dbytes = 10000;
#endif
    double dmbytes = dbytes / (((double)1024) * ((double)1024));
    printf("Starting kernel to read/write %.0lf MB bytes from/to global memory... \n", dmbytes);

    //Write input buffer
    //Map input buffer for PCIe write
    unsigned char *map_input_buffer0;
    map_input_buffer0 = (unsigned char *) clEnqueueMapBuffer(command_queue, 
                                                            input_buffer0, 
                                                            CL_FALSE, 
                                                            CL_MAP_WRITE_INVALIDATE_REGION,
                                                            0, 
                                                            globalbuffersize, 
                                                            0, 
                                                            NULL, 
                                                            NULL, 
                                                            &err);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to clEnqueueMapBuffer OpenCL buffer\n");
        printf("Error: Test failed\n");
        return -1;
    }
    clFinish(command_queue);

    //prepare data to be written to the device
    for(i=0; i<globalbuffersize; i++) 
        map_input_buffer0[i] = input_host[i];

    cl_event event1;
    err = clEnqueueUnmapMemObject(command_queue, 
                                  input_buffer0, 
                                  map_input_buffer0, 
                                  0, 
                                  NULL,
                                  &event1);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to copy input dataset to OpenCL buffer\n");
        printf("Error: Test failed\n");
        return -1;
    }

#ifdef USE_4DDR
    //Map input buffer for PCIe write
    unsigned char *map_input_buffer1;
    map_input_buffer1 = (unsigned char *) clEnqueueMapBuffer(command_queue, 
                                                            input_buffer1, 
                                                            CL_FALSE, 
                                                            CL_MAP_WRITE_INVALIDATE_REGION,
                                                            0, 
                                                            globalbuffersize, 
                                                            0, 
                                                            NULL, 
                                                            NULL, 
                                                            &err);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to clEnqueueMapBuffer OpenCL buffer\n");
        printf("Error: Test failed\n");
        return -1;
    }
    clFinish(command_queue);

    //prepare data to be written to the device
    for(i=0; i<globalbuffersize; i++) 
        map_input_buffer1[i] = input_host[i];

    cl_event event2;
    err = clEnqueueUnmapMemObject(command_queue, 
                                  input_buffer1, 
                                  map_input_buffer1, 
                                  0, 
                                  NULL,
                                  &event2);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to copy input dataset to OpenCL buffer\n");
        printf("Error: Test failed\n");
        return -1;
    }
#endif

    //execute kernel
    int arg_num = 0;
    err  = 0;
    err  = clSetKernelArg(kernel, arg_num++, sizeof(cl_mem), &input_buffer0);
    err |= clSetKernelArg(kernel, arg_num++, sizeof(cl_mem), &output_buffer0);
#ifdef USE_4DDR
    err |= clSetKernelArg(kernel, arg_num++, sizeof(cl_mem), &input_buffer1);
    err |= clSetKernelArg(kernel, arg_num++, sizeof(cl_mem), &output_buffer1);
#endif
    err |= clSetKernelArg(kernel, arg_num++,  sizeof(cl_ulong), &num_blocks);

    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to set kernel arguments! %d\n", err);
        printf("ERROR: Test failed\n");
        return EXIT_FAILURE;
    }

    size_t global[1];
    size_t local[1];
    global[0]=1;
    local[0]=1;

    cl_event ndrangeevent;
    err = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, global, local, 
                                 0, NULL, &ndrangeevent);
    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to execute kernel %d\n", err);
        printf("ERROR: Test failed\n");
        return EXIT_FAILURE;
    }
    
    clFinish(command_queue);

    //copy results back from OpenCL buffer
    unsigned char *map_output_buffer0;
    map_output_buffer0 = (unsigned char *)clEnqueueMapBuffer(command_queue, 
                                                            output_buffer0, 
                                                            CL_FALSE, 
                                                            CL_MAP_READ, 
                                                            0, 
                                                            globalbuffersize, 
                                                            0, 
                                                            NULL, 
                                                            &event1, 
                                                            &err);

    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to read output size buffer %d\n", err);
        printf("ERROR: Test failed\n");
        return EXIT_FAILURE;
    }
    clFinish(command_queue);

    //check
    /*
     for (i=0; i<globalbuffersize; i++) {
        if (map_output_buffer0[i] != input_host[i]) {
            printf("ERROR : kernel failed to copy entry %i input0=%i output0=%i\n",
                   i, input_host[i], map_output_buffer0[i]);
            return EXIT_FAILURE;
       }
    }
    */
#ifdef USE_4DDR
    unsigned char *map_output_buffer1;
    map_output_buffer1 = (unsigned char *)clEnqueueMapBuffer(command_queue, 
                                                            output_buffer1, 
                                                            CL_FALSE, 
                                                            CL_MAP_READ, 
                                                            0, 
                                                            globalbuffersize, 
                                                            0, 
                                                            NULL, 
                                                            &event1, 
                                                            &err);

    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to read output size buffer %d\n", err);
        printf("ERROR: Test failed\n");
        return EXIT_FAILURE;
    }
    clFinish(command_queue);
    
    //check
    for (i=0; i<globalbuffersize; i++) {
        if (map_output_buffer1[i] != input_host[i]) {
            printf("ERROR : kernel failed to copy entry %i input1=%i output1=%i\n",
                   i, input_host[i], map_output_buffer1[i]);
            return EXIT_FAILURE;
        }
     }
#endif


    //--------------------------------------------------------------------------
    //profiling information
    //--------------------------------------------------------------------------
    uint64_t nstimestart, nstimeend;
    clGetEventProfilingInfo(ndrangeevent, CL_PROFILING_COMMAND_START, sizeof(uint64_t), ((void *)(&nstimestart)), NULL);
    clGetEventProfilingInfo(ndrangeevent, CL_PROFILING_COMMAND_END,    sizeof(uint64_t), ((void *)(&nstimeend)),    NULL);
    nsduration = nstimeend-nstimestart;

    double dnsduration = ((double)nsduration);
    double dsduration = dnsduration / ((double) 1000000000);



    //double bpersec = (dbytes*2.0/dsduration);
    double bpersec = (10000*2.0/dsduration);             ////2017.06.29 by Gao Li
    double mbpersec = bpersec / ((double) 1024*1024 );

    printf("Kernel read %.0lf MB bytes from and wrote %.01f MB to global memory.\n", dmbytes, dmbytes);
    printf("Execution time = %f (sec) \n", dsduration);
    printf("Concurrent Read and Write Throughput = %f (MB/sec) \n", mbpersec);

    //--------------------------------------------------------------------------
    //add clena up code
    //--------------------------------------------------------------------------
    clReleaseMemObject(input_buffer0);
    clReleaseMemObject(output_buffer0);
#ifdef USE_4DDR
    clReleaseMemObject(input_buffer1);
    clReleaseMemObject(output_buffer1);
#endif
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);

    return EXIT_SUCCESS;

}

