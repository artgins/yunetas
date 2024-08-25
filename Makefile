# Set the default goal to 'list' so it runs if no target is specified
.DEFAULT_GOAL := list

# Define a list of commands
.PHONY: init-debug init-prod build clean test deploy list

init-debug:
	@echo "Initializing yunetas in Debug mode..."
	@rm -rf build
	@mkdir -p build
	@cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Debug .. && ninja

init-prod:
	@echo "Initializing yunetas in Production mode..."
	@rm -rf build
	@mkdir -p build
	@cd build && cmake -GNinja .. && ninja

build:
	@echo "Building yunetas..."
	@cd build && ninja

clean:
	@echo "Cleaning up yunetas..."
	@rm -rf build

test:
	@echo "Running tests on yunetas..."
	@cd build && ctest

deploy:
	@echo "Deploying yunetas..."
	@cd build && ninja install

list:
	@echo "Available commands:"
	@echo "  make init-debug    - Initialize yunetas in Debug mode"
	@echo "  make init-prod     - Initialize yunetas in Production mode"
	@echo "  make build         - Build yunetas"
	@echo "  make clean         - Clean up generated files from yunetas"
	@echo "  make test          - Run tests on yunetas"
	@echo "  make deploy        - Deploy yunetas"
	@echo "  make list          - List all available commands (default)"
