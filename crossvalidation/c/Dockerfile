FROM alpine:edge

RUN apk add --no-cache gcc musl-dev make bash coreutils

WORKDIR /app/implementations/c

# Library source
COPY implementations/c/include/ ./include/
COPY implementations/c/src/ ./src/
COPY implementations/c/Makefile ./Makefile

# Cross-validation harness source (into the CROSSVAL_DIR expected by Makefile)
COPY crossvalidation/c/ ./crossvalidation/

# Shared validation infrastructure
COPY crossvalidation/run_crossvalidation.sh ./crossvalidation/run_crossvalidation.sh
COPY crossvalidation/file_list.csv ./crossvalidation/file_list.csv

# Build the crossvalidation binaries (but don't run - data mounted at runtime)
RUN make build/crossvalidation_encoder build/crossvalidation_decoder

ENTRYPOINT ["make", "crossvalidation", "CROSSVAL_DATA=/app/ccsds124_full_crossvalidation", "CROSSVAL_RESULTS=build/crossvalidation-results/crossvalidation-results.txt"]
CMD []
