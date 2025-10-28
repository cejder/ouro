package main

import (
	"bufio"
	"flag"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"slices"
	"strings"
	"sync"
)

// Colors for terminal output
const (
	colorReset = "\033[0m"
	colorRed   = "\033[31m"
)

type Result struct {
	Keyword  string
	FilePath string
	LineNum  int
	ColNum   int
	Line     string
}

func main() {
	// Parse command line arguments
	pathPtr := flag.String("path", ".", "Path to the project")
	flag.Parse()

	// Override with positional argument if provided
	if flag.NArg() > 0 {
		*pathPtr = flag.Arg(0)
	}

	// Check if the path exists
	if _, err := os.Stat(*pathPtr); os.IsNotExist(err) {
		fmt.Printf("%sError: Path does not exist.%s\n", colorRed, colorReset)
		os.Exit(1)
	}

	// Define keywords to check
	keywords := []string{
		"dest", "source", "bool", "char", "char16_t", "char32_t", "copy_sign",
		"double", "float", "float_t", "int", "int16_t", "int32_t", "int64_t",
		"int8_t", "int_fast16_t", "int_fast32_t", "int_fast64_t", "int_fast8_t",
		"int_least16_t", "int_least32_t", "int_least64_t", "int_least8_t",
		"intptr_t", "ptrdiff_t", "short", "sig_atomic_t", "signed",
		"size_t", "uint16_t", "uint32_t", "uint64_t", "uint8_t", "uintptr_t",
		"unsigned", "wchar_t", "wint_t", "to_lower", "strtok", "sprintf",
		"vsprintf", "fprintf", "vfprintf", "fflush", "snprintf", "vsnprintf",
		"sscanf", "strcpy", "strchr", "strrchr", "strstr", "strlen", "strcmp",
		"strncmp", "strncat", "strtof", "atoi", "atof", "memset", "memcpy",
		"memmove", "strdup", "strncpy", "strspn", "isspace", "modf", "fmod",
		"fmodf", "fabsf", "fabs", "copysignf", "copysign", "approach",
		"atan2f", "sinf", "cosf", "sqrtf", "floorf", "ceilf", "lerpf", "expf",
		"sin", "powf",
		"PI",
	}

	// File path blacklist
	pathBlacklist := []string{
		"src/common.hpp",
		"src/assert.hpp",
		"src/math.hpp",
		"src/std.cpp",
		"src/log.cpp",
	}

	// String blacklist
	stringBlacklist := []string{
		"fmod.hpp",
		// Add more strings to blacklist here
	}

	// File extension blacklist
	extBlacklist := []string{
		".oudh",
		// Add more file extensions to blacklist here
	}

	// Create regex patterns for each keyword
	patterns := make([]*regexp.Regexp, len(keywords))
	for i, keyword := range keywords {
		patterns[i] = regexp.MustCompile(fmt.Sprintf(`\b%s\b`, keyword))
	}

	// Create a channel to collect results
	resultChan := make(chan Result)

	// Use WaitGroup to track when all goroutines are done
	var wg sync.WaitGroup

	// Number of worker goroutines
	numWorkers := runtime.NumCPU()

	// Channel for passing files to workers
	fileChan := make(chan string, numWorkers)

	// Launch worker goroutines
	for range numWorkers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for filePath := range fileChan {
				processFile(filePath, keywords, patterns, stringBlacklist, resultChan)
			}
		}()
	}

	// Launch a goroutine to close the result channel when all workers are done
	go func() {
		wg.Wait()
		close(resultChan)
	}()

	// Walk the directory and send files to workers
	go func() {
		filepath.WalkDir(*pathPtr, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				return err
			}

			// Skip directories
			if d.IsDir() {
				return nil
			}

			// Check if path is in blacklist
			for _, blacklisted := range pathBlacklist {
				if strings.HasSuffix(path, blacklisted) {
					return nil
				}
			}

			// Check if file extension is blacklisted
			ext := filepath.Ext(path)
			if slices.Contains(extBlacklist, ext) {
				return nil
			}

			fileChan <- path
			return nil
		})
		close(fileChan)
	}()

	// Collect and print results
	issues := false
	resultMap := make(map[string][]Result)

	for result := range resultChan {
		issues = true
		resultMap[result.Keyword] = append(resultMap[result.Keyword], result)
	}

	// Print results in Emacs compile buffer friendly format
	for _, results := range resultMap {
		for _, result := range results {
			// Format: filename:line:column: keyword 'x' found: context
			fmt.Printf("%s:%d:%d: warning: keyword '%s' found: %s\n",
				result.FilePath, result.LineNum, result.ColNum,
				result.Keyword, result.Line)
		}
	}

	if !issues {
		// Simple output without colors for Emacs compatibility
		fmt.Println("No issues found")
	} else {
		// Quiet exit with error code for Emacs compile mode
		os.Exit(1)
	}
}

func processFile(filePath string, keywords []string, patterns []*regexp.Regexp, stringBlacklist []string, resultChan chan<- Result) {
	// Open the file
	file, err := os.Open(filePath)
	if err != nil {
		return
	}
	defer file.Close()

	// Read the file line by line
	scanner := bufio.NewScanner(file)
	lineNum := 0

	for scanner.Scan() {
		lineNum++
		line := scanner.Text()

		// Check if any string from the string blacklist is in the line
		isBlacklisted := false
		for _, blacklistedStr := range stringBlacklist {
			if strings.Contains(line, blacklistedStr) {
				isBlacklisted = true
				break
			}
		}

		if isBlacklisted {
			continue
		}

		// Check each keyword in the line
		for i, pattern := range patterns {
			matches := pattern.FindAllStringIndex(line, -1)
			for _, match := range matches {
				resultChan <- Result{
					Keyword:  keywords[i],
					FilePath: filePath,
					LineNum:  lineNum,
					ColNum:   match[0] + 1, // +1 because editors use 1-based column numbers
					Line:     line,
				}
			}
		}
	}
}
