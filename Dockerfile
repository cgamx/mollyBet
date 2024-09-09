FROM ubuntu:22.04

# Install necessary packages
RUN apt update && apt upgrade -y && apt install -y build-essential gcc g++ cmake git tar zip unzip gzip pkg-config make bash curl ninja-build

# Set the working directory inside the container
WORKDIR /app

# Clone the GitHub repository
RUN git clone https://github.com/cgamx/mollyBet.git

# Change directory into the repository
WORKDIR /app/mollyBet

# Initialize and update git submodules
RUN git submodule update --init

# Create 'linux' directory and change into it
RUN mkdir -p linux && cd linux && cmake .. && cmake --build .

# Change directory into the repository
WORKDIR /app/mollyBet/bin

# Start an interactive shell when the container starts
CMD ["/bin/bash"]
