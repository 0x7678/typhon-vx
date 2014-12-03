import Control.Parallel.OpenCL
import Foreign( castPtr, nullPtr, sizeOf )
import Foreign.C.Types( CULong,CUInt )
import Foreign.Marshal.Array( newArray, peekArray )
import System.Exit( exitFailure )
import qualified Control.Exception as Ex( catch )

main :: IO ()
main = do
    -- Initialize OpenCL
    (platform:_) <- clGetPlatformIDs
    (dev:_) <- clGetDeviceIDs platform CL_DEVICE_TYPE_ALL
    context <- clCreateContext [] [dev] print
    q <- clCreateCommandQueue context dev []
  
    -- Initialize Kernel
    programSource <- readFile "krak.cl"
    program <- clCreateProgramWithSource context programSource
    Ex.catch 
        (clBuildProgram program [dev] "")
        (\CL_BUILD_PROGRAM_FAILURE -> do
            log <- clGetProgramBuildLog program dev
            putStrLn log
            exitFailure
        )
    kernel <- clCreateKernel program "krak"

    -- Initialize parameters
    let states = [ 0x0123456789ABCDEF , 0xFEDCBA9876543210,
                   0x0123456789ABCDEF , 0xFEDCBA9876543210,
                   0x0ff45eeee8843210 , 0xFEDC34ffff543210] :: [CULong]
        elemSize = sizeOf (0::CULong)
        statesCount = length states
        dataSize = elemSize * statesCount
    putStrLn $ "Original array = " ++ show states 
    statesArray  <- newArray states
    
    statesBuff <- clCreateBuffer context [CL_MEM_READ_WRITE, CL_MEM_COPY_HOST_PTR] (dataSize, castPtr statesArray)
    
    let localSize = div statesCount 2
        pStates = (fromIntegral $ statesCount) :: CUInt
        pRounds = 4 :: CUInt
        pControl = 0 :: CUInt -- unsued

    clSetKernelArgSto kernel 0 statesBuff
    clSetKernelArgSto kernel 1 pStates
    clSetKernelArgSto kernel 2 pRounds
    clSetKernelArgSto kernel 3 pControl

    -- Execute Kernel
    eventExec <- clEnqueueNDRangeKernel q kernel [localSize,1,1] [1,1,1] []

    -- Get Result
    eventRead <- clEnqueueReadBuffer q statesBuff True 0 dataSize (castPtr statesArray) [eventExec]
    result <- peekArray statesCount statesArray
    putStrLn $ "Result array   = " ++ show result

    return ()



