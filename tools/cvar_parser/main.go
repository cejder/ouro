package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

type CVarType string

const (
	CVarTypeBOOL    = "BOOL"
	CVarTypeS32     = "S32"
	CVarTypeF32     = "F32"
	CVarTypeCVarStr = "CVarStr"
)

func inferCVarType(value string) CVarType {
	value = strings.TrimSpace(value)
	lowerValue := strings.ToLower(value)

	if lowerValue == "true" || lowerValue == "false" {
		return CVarTypeBOOL
	}

	if _, err := strconv.ParseFloat(value, 32); err == nil {
		if strings.Contains(value, ".") || strings.Contains(value, "e") || strings.Contains(value, "E") {
			return CVarTypeF32
		}
	}

	if _, err := strconv.ParseInt(value, 10, 32); err == nil {
		return CVarTypeS32
	}

	return CVarTypeCVarStr
}

const CVarSeparator = ":"

type CVar struct {
	Type    CVarType
	Name    string
	Value   string
	Comment string // Optional comment for this variable
}

func main() {
	if len(os.Args) != 3 {
		fmt.Println("Usage: program <input_file> <output_file>")
		os.Exit(1)
	}

	inputFile := os.Args[1]
	outputFile := os.Args[2]

	if inputFile == "" {
		fmt.Fprintf(os.Stderr, "Input file is required\n")
		os.Exit(1)
	}

	if _, err := os.Stat(inputFile); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Input file does not exist: %s\n", inputFile)
		os.Exit(1)
	}

	cvars, err := parseFile(inputFile)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)
		os.Exit(1)
	}

	// Get working directory to create relative paths
	pwd, err := os.Getwd()
	if err != nil {
		pwd = "."
	}

	// Convert input path to relative
	relInput, err := filepath.Rel(pwd, inputFile)
	if err != nil {
		relInput = inputFile
	}

	// Convert output paths to relative
	relOutputHpp, err := filepath.Rel(pwd, outputFile+".hpp")
	if err != nil {
		relOutputHpp = outputFile + ".hpp"
	}
	relOutputCpp, err := filepath.Rel(pwd, outputFile+".cpp")
	if err != nil {
		relOutputCpp = outputFile + ".cpp"
	}

	// Trim prefix that might be ../../
	relInput = strings.TrimPrefix(relInput, "../../")
	relOutputHpp = strings.TrimPrefix(relOutputHpp, "../../")
	relOutputCpp = strings.TrimPrefix(relOutputCpp, "../../")

	const (
		INPUT  = "\033[0;33m"
		OUTPUT = "\033[0;32m"
		COUNT  = "\033[0;35m"
		ARROW  = "\033[0;36m"
		NC     = "\033[0m"
	)

	fmt.Printf("%s[%d cvars]%s  %s%s%s %s->%s %s%s%s, %s%s%s\n",
		COUNT, len(cvars), NC,
		INPUT, relInput, NC,
		ARROW, NC,
		OUTPUT, relOutputHpp, NC,
		OUTPUT, relOutputCpp, NC)

	maxNameWidth := 0
	for _, cvar := range cvars {
		if len(cvar.Name) > maxNameWidth {
			maxNameWidth = len(cvar.Name)
		}
	}

	for _, cvar := range cvars {
		if cvar.Comment != "" {
			fmt.Printf("  %s%-*s%s : %s%s%s (%s%s%s) # %s\n",
				OUTPUT, maxNameWidth, cvar.Name, NC,
				INPUT, cvar.Value, NC,
				COUNT, cvar.Type, NC,
				cvar.Comment)
		} else {
			fmt.Printf("  %s%-*s%s : %s%s%s (%s%s%s)\n",
				OUTPUT, maxNameWidth, cvar.Name, NC,
				INPUT, cvar.Value, NC,
				COUNT, cvar.Type, NC)
		}
	}

	if err := generateCode(outputFile, cvars); err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)
		os.Exit(1)
	}
}

func parseFile(filename string) ([]CVar, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	var cvars []CVar
	var currentPrefix string
	seenNames := make(map[string]int) // Track seen variable names and their line numbers

	scanner := bufio.NewScanner(file)
	lineNumber := 0
	for scanner.Scan() {
		lineNumber++

		line := scanner.Text()
		var comment string

		// Extract comment if present
		if idx := strings.Index(line, "#"); idx != -1 {
			comment = strings.TrimSpace(line[idx+1:])
			line = line[:idx]
		}
		line = strings.TrimSpace(line)

		if line == "" {
			continue
		}

		// Check for header section [header_name]
		if strings.HasPrefix(line, "[") && strings.HasSuffix(line, "]") {
			header := strings.TrimSpace(line[1 : len(line)-1])
			if header == "" {
				return nil, fmt.Errorf("%s:%d: empty header name in %q", filename, lineNumber, line)
			}
			currentPrefix = header
			continue
		}

		split := strings.Split(line, ":")
		if len(split) != 2 {
			return nil, fmt.Errorf("%s:%d: line '%s' must have exactly one ':' separating variable name and value", filename, lineNumber, line)
		}

		var cvar CVar

		varName := strings.TrimSpace(split[0])
		if varName == "" {
			return nil, fmt.Errorf("%s:%d: %q has no variable name", filename, lineNumber, line)
		}

		// Require that we have a header - no headerless variables allowed
		if currentPrefix == "" {
			return nil, fmt.Errorf("%s:%d: variable '%s' must be under a header section like [audio], [debug], [video], etc.", filename, lineNumber, varName)
		}

		cvar.Name = currentPrefix + "__" + varName
		cvar.Comment = comment

		// Check for duplicates
		if prevLine, exists := seenNames[cvar.Name]; exists {
			fmt.Fprintf(os.Stderr, "WARNING: %s:%d: duplicate variable %q (previously defined at line %d)\n", filename, lineNumber, cvar.Name, prevLine)
		}
		seenNames[cvar.Name] = lineNumber

		cvar.Value = strings.TrimSpace(split[1])
		if cvar.Value == "" {
			return nil, fmt.Errorf("%s:%d: variable '%s' has no value (format should be: %s : value)", filename, lineNumber, varName, varName)
		}

		cvar.Type = inferCVarType(cvar.Value)

		cvars = append(cvars, cvar)
	}

	// Sort cvars by name
	sort.Slice(cvars, func(i int, j int) bool {
		return cvars[i].Name < cvars[j].Name
	})

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}

	return cvars, nil
}

func generateCode(outputFile string, cvars []CVar) error {
	// Generate header file
	if err := generateFromTemplate("cvar.hpp.tpl", outputFile+".hpp", outputFile, cvars); err != nil {
		return err
	}

	// Generate source file
	if err := generateFromTemplate("cvar.cpp.tpl", outputFile+".cpp", outputFile, cvars); err != nil {
		return err
	}

	return nil
}

func generateFromTemplate(templateFile, outputFile, outputFileBase string, cvars []CVar) error {
	// Read template file
	templateData, err := os.ReadFile(templateFile)
	if err != nil {
		return err
	}

	content := string(templateData)

	// Replace placeholders
	content = strings.ReplaceAll(content, "{{CVAR_COUNT}}", fmt.Sprintf("%d", len(cvars)))
	content = strings.ReplaceAll(content, "{{OUTPUT_FILE_BASE}}", filepath.Base(outputFileBase))

	// Calculate longest lengths
	longestNameLength := 0
	longestVarNameLength := 0
	longestValueLength := 0
	longestTypeLength := 0
	longestPrefixLength := 0
	for _, cvar := range cvars {
		if len(cvar.Name) > longestNameLength {
			longestNameLength = len(cvar.Name)
		}
		if len(cvar.Value) > longestValueLength {
			longestValueLength = len(cvar.Value)
		}
		if len(cvar.Type) > longestTypeLength {
			longestTypeLength = len(cvar.Type)
		}

		// Extract variable name (after double underscore) for save formatting
		if idx := strings.Index(cvar.Name, "__"); idx != -1 {
			varName := cvar.Name[idx+2:]
			if len(varName) > longestVarNameLength {
				longestVarNameLength = len(varName)
			}
			prefix := cvar.Name[:idx]
			if len(prefix) > longestPrefixLength {
				longestPrefixLength = len(prefix)
			}
		}
	}

	content = strings.ReplaceAll(content, "{{CVAR_LONGEST_NAME_LENGTH}}", fmt.Sprintf("%d", longestNameLength))
	content = strings.ReplaceAll(content, "{{CVAR_LONGEST_VAR_NAME_LENGTH}}", fmt.Sprintf("%d", longestVarNameLength))
	content = strings.ReplaceAll(content, "{{CVAR_LONGEST_VALUE_LENGTH}}", fmt.Sprintf("%d", longestValueLength))
	content = strings.ReplaceAll(content, "{{CVAR_LONGEST_TYPE_LENGTH}}", fmt.Sprintf("%d", longestTypeLength))
	content = strings.ReplaceAll(content, "{{CVAR_LONGEST_PREFIX_LENGTH}}", fmt.Sprintf("%d", longestPrefixLength))

	// Generate cvar declarations (for header)
	var declarations strings.Builder
	for i, cvar := range cvars {
		fmt.Fprintf(&declarations, "extern %-*s c_%s;", longestTypeLength, cvar.Type, cvar.Name)

		if i != len(cvars)-1 {
			declarations.WriteString("\n")
		}
	}
	content = strings.ReplaceAll(content, "{{CVAR_DECLARATIONS}}", declarations.String())

	// Generate cvar definitions (for source)
	var definitions strings.Builder
	for i, cvar := range cvars {
		switch cvar.Type {
		case CVarTypeBOOL:
			fmt.Fprintf(&definitions, "%-*s c_%-*s = %s;", longestTypeLength, cvar.Type, longestNameLength, cvar.Name, strings.ToLower(cvar.Value))
		case CVarTypeS32:
			fmt.Fprintf(&definitions, "%-*s c_%-*s = %s;", longestTypeLength, cvar.Type, longestNameLength, cvar.Name, cvar.Value)
		case CVarTypeF32:
			fmt.Fprintf(&definitions, "%-*s c_%-*s = %sF;", longestTypeLength, cvar.Type, longestNameLength, cvar.Name, cvar.Value)
		case CVarTypeCVarStr:
			fmt.Fprintf(&definitions, "%-*s c_%-*s = {\"%s\"};", longestTypeLength, cvar.Type, longestNameLength, cvar.Name, cvar.Value)
		}

		if i != len(cvars)-1 {
			definitions.WriteString("\n")
		}
	}
	content = strings.ReplaceAll(content, "{{CVAR_DEFINITIONS}}", definitions.String())

	// Generate meta table (for source)
	var metaTable strings.Builder
	for i, cvar := range cvars {
		fmt.Fprintf(&metaTable, "    {\"%s\",%*s &c_%s,%*s CVAR_TYPE_%s,%*s \"%s\"}",
			cvar.Name,
			longestNameLength-len(cvar.Name)+1, "",
			cvar.Name,
			longestNameLength-len(cvar.Name)+1, "",
			strings.ToUpper(string(cvar.Type)),
			longestTypeLength-len(strings.ToUpper(string(cvar.Type)))+1, "",
			cvar.Comment)

		if i != len(cvars)-1 {
			metaTable.WriteString(",\n")
		}
	}
	content = strings.ReplaceAll(content, "{{CVAR_META_TABLE}}", metaTable.String())

	// Write output file
	return os.WriteFile(outputFile, []byte(content), 0o644)
}
