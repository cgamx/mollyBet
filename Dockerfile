# Use a lightweight Alpine base image
FROM alpine:3.18

# Install necessary packages: g++, cmake, git
RUN apk add --no-cache gcc g++ cmake git zip pkgconfig

# Set the working directory inside the container
WORKDIR /app

# Clone the GitHub repository (replace with your repository URL)
RUN git clone https://github.com/cgamx/mollyBet.git

# Change directory into the repository
WORKDIR /app/mollyBet

# Initialize and update git submodules
RUN git submodule update --init

# Create 'linux' directory and change into it
RUN mkdir -p linux && cd linux && cmake ..

# Build the project
RUN cd linux && cmake --build .

# Start an interactive shell when the container starts (optional)
CMD ["/bin/sh"]
