FROM python:3.11-alpine

WORKDIR /app/implementations/python

COPY implementations/python/ ./
COPY test-vectors/ /app/test-vectors/

RUN pip install --no-cache-dir -e ".[dev]"

# Default: lint, type check, test with coverage and HTML reports, generate API docs
CMD ["sh", "-c", "\
    ruff format --check . && \
    ruff check . && \
    mypy ccsds124 && \
    mkdir -p build/docs && \
    pytest -v --cov=ccsds124 \
        --cov-report=html:build/docs/coverage \
        --cov-report=xml:build/docs/coverage.xml \
        --html=build/docs/tests/index.html --self-contained-html && \
    pdoc -d google ccsds124 -o build/docs/api \
"]
