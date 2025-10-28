package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)

type Node struct {
	nodeType  string
	text      string
	next      string
	responses []Response
	label     string
}

type Response struct {
	text    string
	next    string
	trigger string
}

type FileTask struct {
	inputFile  string
	outputFile string
}

type ProcessResult struct {
	inputFile string
	err       error
}

type CacheEntry struct {
	ModTime time.Time
}

func loadMetaCache(metaPath string) (map[string]CacheEntry, error) {
	cache := make(map[string]CacheEntry)

	file, err := os.Open(metaPath)
	if err != nil {
		return cache, err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		parts := strings.SplitN(scanner.Text(), "|", 2)
		if len(parts) != 2 {
			continue
		}

		modTime, err := time.Parse(time.RFC3339, parts[1])
		if err != nil {
			continue
		}

		cache[parts[0]] = CacheEntry{ModTime: modTime}
	}

	return cache, scanner.Err()
}

func saveMetaCache(metaPath string, cache map[string]CacheEntry) error {
	file, err := os.Create(metaPath)
	if err != nil {
		return err
	}
	defer file.Close()

	for path, entry := range cache {
		if _, err := fmt.Fprintf(file, "%s|%s\n", filepath.Base(path), entry.ModTime.Format(time.RFC3339)); err != nil {
			return err
		}
	}

	return nil
}

const (
	META   = "meta.txt"
	INFO   = "\033[0;36m"
	ARROW  = "\033[0;36m"
	INPUT  = "\033[0;33m"
	OUTPUT = "\033[0;32m"
	COUNT  = "\033[0;35m"
	NC     = "\033[0m"
)

func main() {
	if len(os.Args) != 4 {
		fmt.Println("Usage: program <input_dir> <output_dir> <cache_dir>")
		os.Exit(1)
	}

	inputDir := os.Args[1]
	outputDir := os.Args[2]
	cacheDir := os.Args[3]

	// Verify input directory exists
	if _, err := os.Stat(inputDir); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Input directory does not exist: %s\n", inputDir)
		os.Exit(1)
	}

	// Create output directory if it doesn't exist
	if err := os.MkdirAll(outputDir, 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "Error creating output directory %s: %v\n", outputDir, err)
		os.Exit(1)
	}

	// Create cache directory if it doesn't exist
	if err := os.MkdirAll(cacheDir, 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "Error creating cache directory %s: %v\n", cacheDir, err)
		os.Exit(1)
	}

	// Get all .oud files from input directory
	files, err := filepath.Glob(filepath.Join(inputDir, "*.oud"))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error finding .oud files: %v\n", err)
		os.Exit(1)
	}
	if len(files) == 0 {
		fmt.Fprintf(os.Stderr, "No .oud files found in %s\n", inputDir)
		os.Exit(1)
	}

	// Read cached meta info
	metaPath := filepath.Join(cacheDir, META)
	cache, err := loadMetaCache(metaPath)
	if err != nil {
		fmt.Printf("%sNo %s found or error reading it, fresh run%s\n", INFO, META, NC)
		cache = make(map[string]CacheEntry)
	}

	// Filter files that need processing
	var filesToProcess []string
	newCache := make(map[string]CacheEntry)

	for _, file := range files {
		filename := filepath.Base(file)

		fileInfo, err := os.Stat(file)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error getting file info for %s: %v\n", file, err)
			continue
		}

		newCache[filename] = CacheEntry{ModTime: fileInfo.ModTime()}

		if cached, exists := cache[filename]; !exists || cached.ModTime.Unix() != fileInfo.ModTime().Unix() {
			filesToProcess = append(filesToProcess, file)
		} else {
			// Get working directory to create relative paths
			pwd, err := os.Getwd()
			if err != nil {
				pwd = "."
			}

			// Convert input path to relative
			relInput, err := filepath.Rel(pwd, file)
			if err != nil {
				relInput = file
			}

			fmt.Printf("%sSkipping %s (unchanged)%s\n", INFO, relInput, NC)
		}
	}

	// Process only modified files
	if len(filesToProcess) > 0 {
		results := processFilesParallel(filesToProcess, outputDir)

		// Report errors and log created files
		hasErrors := false
		for i, result := range results {
			if result.err != nil {
				fmt.Fprintf(os.Stderr, "Error processing %s: %v\n", result.inputFile, result.err)
				hasErrors = true
			} else {
				// Get working directory to create relative paths
				pwd, err := os.Getwd()
				if err != nil {
					pwd = "."
				}

				// Convert input path to relative
				relInput, err := filepath.Rel(pwd, result.inputFile)
				if err != nil {
					relInput = result.inputFile
				}

				// Construct output path
				baseName := filepath.Base(result.inputFile)
				outputFile := filepath.Join(outputDir, strings.TrimSuffix(baseName, ".oud")+".oudh")
				relOutput, err := filepath.Rel(pwd, outputFile)
				if err != nil {
					relOutput = outputFile
				}

				// Trim prefix that might be ../../
				relInput = strings.TrimPrefix(relInput, "../../")
				relOutput = strings.TrimPrefix(relOutput, "../../")

				fmt.Printf("%s[%d/%d]%s  %s%s%s %s->%s %s%s%s\n",
					COUNT, i+1, len(results), NC,
					INPUT, relInput, NC,
					ARROW, NC,
					OUTPUT, relOutput, NC)
			}
		}

		if hasErrors {
			os.Exit(1)
		}
	} else {
		fmt.Printf("%sAll files are up to date%s\n", INFO, NC)
	}

	// Save the new cache
	if err := saveMetaCache(metaPath, newCache); err != nil {
		fmt.Fprintf(os.Stderr, "Error saving cache: %v\n", err)
		os.Exit(1)
	}
}

func processFilesParallel(inputFiles []string, outputDir string) []ProcessResult {
	numWorkers := runtime.NumCPU()
	tasks := make(chan FileTask, len(inputFiles))
	results := make(chan ProcessResult, len(inputFiles))
	var wg sync.WaitGroup

	// Start workers
	for range numWorkers {
		wg.Add(1)
		go worker(&wg, tasks, results)
	}

	// Send tasks to workers
	for _, inputFile := range inputFiles {
		baseName := filepath.Base(inputFile)
		outputFile := filepath.Join(outputDir, strings.TrimSuffix(baseName, ".oud")+".oudh")
		tasks <- FileTask{inputFile: inputFile, outputFile: outputFile}
	}
	close(tasks)

	// Wait for all workers to finish
	wg.Wait()
	close(results)

	// Collect all results
	var processResults []ProcessResult
	for result := range results {
		processResults = append(processResults, result)
	}

	return processResults
}

func worker(wg *sync.WaitGroup, tasks <-chan FileTask, results chan<- ProcessResult) {
	defer wg.Done()

	for task := range tasks {
		dialogName, nodes, err := parseFile(task.inputFile)
		if err != nil {
			results <- ProcessResult{inputFile: task.inputFile, err: err}
			continue
		}

		if err := generateCode(dialogName, nodes, task.outputFile); err != nil {
			results <- ProcessResult{inputFile: task.inputFile, err: fmt.Errorf("error generating code: %v", err)}
			continue
		}

		results <- ProcessResult{inputFile: task.inputFile, err: nil}
	}
}

func parseFile(filename string) (string, map[string]*Node, error) {
	file, err := os.Open(filename)
	if err != nil {
		return "", nil, err
	}
	defer file.Close()

	nodes := make(map[string]*Node)
	var currentNode *Node
	var dialogName string

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, ";;") {
			continue
		}

		if strings.HasPrefix(line, "(name") {
			fields := strings.Fields(strings.TrimRight(line, ")"))
			if len(fields) >= 2 {
				dialogName = fields[1]
			}
			continue
		}

		if strings.HasPrefix(line, "(node") {
			fields := strings.Fields(strings.TrimRight(line, ")"))
			if len(fields) >= 3 {
				label := fields[2]
				if label == "start" {
					label = "T_START"
				}
				currentNode = &Node{
					nodeType: fields[1],
					label:    label,
				}
				nodes[currentNode.label] = currentNode
			}
			continue
		}

		if strings.HasPrefix(line, "(text") {
			textStart := strings.Index(line, "\"")
			textEnd := strings.LastIndex(line, "\"")
			if textStart != -1 && textEnd != -1 && textEnd > textStart {
				text := line[textStart+1 : textEnd]
				fields := strings.Fields(line[textEnd+1:])
				nextNode := fields[len(fields)-1]
				nextNode = strings.TrimRight(nextNode, ")")
				if nextNode == "quit" {
					nextNode = "T_QUIT"
				}
				currentNode.text = text
				currentNode.next = nextNode
			}
			continue
		}

		if strings.HasPrefix(line, "(resp") || strings.HasPrefix(line, "(trig") {
			textStart := strings.Index(line, "\"")
			textEnd := strings.LastIndex(line, "\"")
			if textStart != -1 && textEnd != -1 && textEnd > textStart {
				text := line[textStart+1 : textEnd]
				fields := strings.Fields(line[textEnd+1:])
				fields = strings.Split(strings.TrimRight(strings.Join(fields, " "), ")"), " ")

				resp := Response{text: text}

				if strings.HasPrefix(line, "(resp") {
					resp.next = fields[0]
				} else {
					resp.next = fields[0]
					resp.trigger = fields[1]
				}

				if resp.next == "quit" {
					resp.next = "T_QUIT"
				}

				currentNode.responses = append(currentNode.responses, resp)
			}
			continue
		}

		return "", nil, fmt.Errorf("This line seems fishy: %s", line)
	}

	return dialogName, nodes, nil
}

func generateCode(dialogName string, nodes map[string]*Node, outputFile string) error {
	file, err := os.Create(outputFile)
	if err != nil {
		return err
	}
	defer file.Close()

	fmt.Fprintf(file, "T_INIT(%s, %d);\n", dialogName, len(nodes))

	if start, ok := nodes["T_START"]; ok {
		writeNode(file, start, dialogName)
		delete(nodes, "T_START")
	}

	for _, node := range nodes {
		writeNode(file, node, dialogName)
	}

	return nil
}

func writeNode(file *os.File, node *Node, dialogName string) {
	switch node.nodeType {
	case "s":
		fmt.Fprintf(file, "T_STATEMENT(%s, %s, \"%s\", %s);\n",
			dialogName, node.label, node.text, node.next)
	case "q":
		fmt.Fprintf(file, "T_QUESTION(%s, %s, \"%s\", %d);\n",
			dialogName, node.label, node.text, len(node.responses))
		for i, resp := range node.responses {
			if resp.trigger != "" {
				fmt.Fprintf(file, "T_RESPONSE_TRIGGER(%s, %s, trigger_%d, \"%s\", %s, cb_trigger_%s, nullptr);\n",
					dialogName, node.label, i, resp.text, resp.next, resp.trigger)
			} else {
				fmt.Fprintf(file, "T_RESPONSE(%s, %s, option_%d, \"%s\", %s);\n",
					dialogName, node.label, i, resp.text, resp.next)
			}
		}
	}
}
