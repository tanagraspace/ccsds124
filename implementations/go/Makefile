.PHONY: all build test test-race coverage coverage-html test-report docs fmt fmt-check vet clean cli

BUILD_DIR = build
DOCS_DIR = $(BUILD_DIR)/docs

all: build

build:
	go build ./...

cli:
	mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/ccsds124 ./cmd/ccsds124/

test:
	go test -v ./...

test-race:
	go test -v -race ./...

test-report:
	@mkdir -p $(DOCS_DIR)/tests
	@echo "Running tests and generating report..."
	@go test -json ./ccsds124/... | go-test-report -o $(DOCS_DIR)/tests/index.html -t "CCSDS 124.0-B-1 Go Test Report"

coverage:
	mkdir -p $(BUILD_DIR)
	go test -coverprofile=$(BUILD_DIR)/coverage.out ./ccsds124/...
	go tool cover -func=$(BUILD_DIR)/coverage.out

coverage-html:
	mkdir -p $(DOCS_DIR)/coverage
	go test -coverprofile=$(BUILD_DIR)/coverage.out ./ccsds124/...
	go tool cover -html=$(BUILD_DIR)/coverage.out -o $(DOCS_DIR)/coverage/index.html

docs: coverage-html test-report api-docs

api-docs:
	@mkdir -p $(DOCS_DIR)/api
	@echo "Generating API documentation..."
	@./scripts/generate-api-docs.sh $(DOCS_DIR)/api

fmt:
	gofmt -w .

fmt-check:
	@test -z "$$(gofmt -l .)" || (echo "Code is not formatted. Run 'make fmt'" && gofmt -l . && exit 1)

vet:
	go vet ./...

clean:
	rm -rf $(BUILD_DIR)/* 2>/dev/null || true
	rmdir $(BUILD_DIR) 2>/dev/null || true
