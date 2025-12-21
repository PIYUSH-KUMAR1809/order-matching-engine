# Delete current build directory
rm -rf ./build

# Create new build directory
mkdir -p build
cd build

# Generate MakeFile
cmake ..

# Create executable
make

# Run benchmark
./src/benchmark