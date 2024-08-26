all:
	 clang++ -fprofile-use=default.profdata -o sloth glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
	-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -lm -fuse-ld=lld  \
	-mpopcnt -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx -march=native -mtune=native
	
	
	
	
	
	#       -mpopcnt -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx -march=native -mtune=native      <    avx/bmi2 enable
	
	
	#       -fprofile-instr-generate -fcoverage-mapping                                                 <   before -o
	
	#        llvm-profdata merge -output=default.profdata *.profraw                                     <  enter on command line
 
    #       -fprofile-use=default.profdata                                                              <   before -o
	
	

    
    #       -march=silvermont -mtune=silvermont  -mpopcnt -msse4.1 -msse4.2                             <  for popcount builds (with sse4.1/4.2)    


    #       -msse3 -mssse3                                                                              <  sse3 build	
   
