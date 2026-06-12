package ccsds124

import (
	"os"
	"path/filepath"
	"testing"
)

func BenchmarkCompressSimple(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "simple.bin"))
	if err != nil {
		b.Skip("Could not load simple.bin")
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Compress(input, 90, 1, 10, 20, 50)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDecompressSimple(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "simple.bin"))
	if err != nil {
		b.Skip("Could not load simple.bin")
	}

	compressed, err := Compress(input, 90, 1, 10, 20, 50)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Decompress(compressed, 90, 1)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkCompressHiro(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "hiro.bin"))
	if err != nil {
		b.Skip("Could not load hiro.bin")
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Compress(input, 90, 7, 10, 20, 50)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDecompressHiro(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "hiro.bin"))
	if err != nil {
		b.Skip("Could not load hiro.bin")
	}

	compressed, err := Compress(input, 90, 7, 10, 20, 50)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Decompress(compressed, 90, 7)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkCompressHousekeeping(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "housekeeping.bin"))
	if err != nil {
		b.Skip("Could not load housekeeping.bin")
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Compress(input, 90, 2, 20, 50, 100)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDecompressHousekeeping(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "housekeeping.bin"))
	if err != nil {
		b.Skip("Could not load housekeeping.bin")
	}

	compressed, err := Compress(input, 90, 2, 20, 50, 100)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Decompress(compressed, 90, 2)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkCompressVenusExpress(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "venus-express.ccsds"))
	if err != nil {
		b.Skip("Could not load venus-express.ccsds")
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Compress(input, 90, 2, 20, 50, 100)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDecompressVenusExpress(b *testing.B) {
	input, err := os.ReadFile(filepath.Join(getTestVectorsPath(), "input", "venus-express.ccsds"))
	if err != nil {
		b.Skip("Could not load venus-express.ccsds")
	}

	compressed, err := Compress(input, 90, 2, 20, 50, 100)
	if err != nil {
		b.Fatal(err)
	}

	b.ResetTimer()
	b.SetBytes(int64(len(input)))

	for i := 0; i < b.N; i++ {
		_, err := Decompress(compressed, 90, 2)
		if err != nil {
			b.Fatal(err)
		}
	}
}
