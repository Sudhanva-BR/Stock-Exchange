# Build stage
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the project files
COPY . .

# Build the project (Release mode)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release

# Runtime stage
FROM ubuntu:24.04

# Install necessary runtime libraries
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from the builder
COPY --from=builder /app/build/MiniExchangeServer .

# Expose the port (Render uses the PORT environment variable by default, but we'll default to 8080)
EXPOSE 8080

# Run the server
# We use a shell to allow parsing of $PORT if Render provides one, otherwise default to 8080
CMD ./MiniExchangeServer ${PORT:-8080}
