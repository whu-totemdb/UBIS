#!/bin/bash

# Global variable for UBIS executable path
UBIS_EXECUTABLE="ubis"

# Default values
INDEX_DIRECTORY=""
DIM=""
VECTOR_PATH=""
QUERY_PATH=""
HEAD_INDEX_FOLDER=""
POSTING_PAGE_LIMIT=""
KV_PATH=""
RESULT_NUM=""
START_NUM=""
STEP=""
TERMINATE_NUM=""
INSERT_THREAD_NUM=""
APPEND_THREAD_NUM=""
BALANCE_FACTOR=""
BASE_VECTOR_SPLIT_PATH=""
FULL_VECTOR_PATH=""
TRUTH_FILE_PREFIX=""

# Show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Required Options:
  -i INDEX_DIR      Index config file directory

Configuration Options:
  -d DIM            Vector dimension
  -v VECTOR_PATH    Base vector file path
  -q QUERY_PATH     Query vector file path
  -H HEAD_FOLDER    Head index folder name
  -p PAGE_LIMIT     Posting page limit
  -k KV_PATH        KV storage path (RocksDB)
  -r RESULT_NUM     Result number
  -s START_NUM      Start number
  -t STEP           Step size
  -e TERMINATE_NUM  Terminate number
  -j INSERT_THREAD  Insert thread number
  -a APPEND_THREAD  Append thread number
  -b BALANCE_FACTOR Balance factor
  -l SPLIT_PATH     Base vector split path
  -f FULL_PATH      Full vector path
  -u TRUTH_PREFIX   Truth file prefix

Execution Options:
  -U UBIS_PATH      Set UBIS executable path (default: ubis)

  -h, --help           Show this help message

Example:
  $0 -i /path/to/index/dir -d 768 -v /path/base.bin -k /path/to/rocksdb -U /custom/path/ubis

  $0 -i /path/to/index/dir -d 1024 -U /custom/path/ubis
EOF
  exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -i)
      INDEX_DIRECTORY="$2"
      shift 2
      ;;
    -d)
      DIM="$2"
      shift 2
      ;;
    -v)
      VECTOR_PATH="$2"
      shift 2
      ;;
    -q)
      QUERY_PATH="$2"
      shift 2
      ;;
    -H)
      HEAD_INDEX_FOLDER="$2"
      shift 2
      ;;
    -p)
      POSTING_PAGE_LIMIT="$2"
      shift 2
      ;;
    -k)
      KV_PATH="$2"
      shift 2
      ;;
    -r)
      RESULT_NUM="$2"
      shift 2
      ;;
    -s)
      START_NUM="$2"
      shift 2
      ;;
    -t)
      STEP="$2"
      shift 2
      ;;
    -e)
      TERMINATE_NUM="$2"
      shift 2
      ;;
    -j)
      INSERT_THREAD_NUM="$2"
      shift 2
      ;;
    -a)
      APPEND_THREAD_NUM="$2"
      shift 2
      ;;
    -b)
      BALANCE_FACTOR="$2"
      shift 2
      ;;
    -l)
      BASE_VECTOR_SPLIT_PATH="$2"
      shift 2
      ;;
    -f)
      FULL_VECTOR_PATH="$2"
      shift 2
      ;;
    -u)
      TRUTH_FILE_PREFIX="$2"
      shift 2
      ;;
    -U)
      UBIS_EXECUTABLE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "Error: Unknown option $1"
      usage
      ;;
  esac
done

# Validate required parameters
validate_parameters() {
  if [[ -z "$INDEX_DIRECTORY" ]]; then
    echo "Error: Index directory is required (-i)"
    usage
  fi
  
}

# Check if UBIS executable exists and is executable
check_ubis_executable() {
  if ! command -v "$UBIS_EXECUTABLE" &> /dev/null; then
    echo "Error: UBIS executable not found or not executable: $UBIS_EXECUTABLE"
    echo "Please check the path or use -U option to specify the correct path"
    return 1
  fi
  echo "UBIS executable: $UBIS_EXECUTABLE"
  return 0
}

# Run UBIS executable with the config file
run_ubis() {
  local config_file="$1"
  
  echo ""
  echo "================================"
  echo "Running UBIS executable..."
  echo "Command: $UBIS_EXECUTABLE \"$config_file\""
  echo "------------------------"
  
  # Check if executable exists
  if ! check_ubis_executable; then
    echo "Skipping UBIS execution"
    return 1
  fi
  
  # Run UBIS with the config file
  echo "Starting UBIS with config: $config_file"
  echo "Timestamp: $(date)"
  echo ""
  
  # Execute UBIS
  if "$UBIS_EXECUTABLE" "$config_file"; then
    echo ""
    echo "UBIS execution completed successfully"
    return 0
  else
    echo ""
    echo "UBIS execution failed with exit code $?"
    return 1
  fi
}

# Calculate derived paths based on IndexDirectory and HeadIndexFolder
calculate_derived_paths() {
  local index_dir="$1"
  local head_folder="$2"
  local config_file="$3"
  
  # Only calculate if IndexDirectory is being modified
  if [[ -n "$index_dir" && -n "$head_folder" ]]; then
    local deleted_ids="${index_dir}/${head_folder}/VectorVersionLabel.bin"
    local posting_label="${index_dir}/${head_folder}/PostingVersionLabel.bin"
    local ssd_info="${index_dir}/${head_folder}/SsdInfoFile.bin"
    
    # Update DerivedIDs if it exists in config
    if grep -q "^DeletedIDs=" "$config_file"; then
      sed -i "s|^DeletedIDs=.*|DeletedIDs=${deleted_ids}|" "$config_file"
      echo "✓ Updated DeletedIDs = ${deleted_ids}"
    fi
    
    # Update PostingVersionLabelFile if it exists in config
    if grep -q "^PostingVersionLabelFile=" "$config_file"; then
      sed -i "s|^PostingVersionLabelFile=.*|PostingVersionLabelFile=${posting_label}|" "$config_file"
      echo "✓ Updated PostingVersionLabelFile = ${posting_label}"
    fi
    
    # Update SsdInfoFile if it exists in config
    if grep -q "^SsdInfoFile=" "$config_file"; then
      sed -i "s|^SsdInfoFile=.*|SsdInfoFile=${ssd_info}|" "$config_file"
      echo "✓ Updated SsdInfoFile = ${ssd_info}"
    fi
  fi
}

# Read value from INI configuration file
read_ini_value() {
  local file_path="$1"
  local section="$2"
  local key="$3"
  
  if [[ ! -f "$file_path" ]]; then
    echo "Error: Config file does not exist: $file_path"
    return 1
  fi
  
  local in_section=false
  local value=""
  
  while IFS= read -r line; do
    # Remove leading/trailing whitespace and comments
    line=$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' | sed 's/;.*$//')
    
    # Skip empty lines
    [[ -z "$line" ]] && continue
    
    # Check if entering target section
    if [[ "$line" =~ ^\[${section}\]$ ]]; then
      in_section=true
      continue
    elif [[ "$line" =~ ^\[.*\]$ ]] && [[ "$in_section" == true ]]; then
      # Entered another section, exit target section
      in_section=false
      continue
    fi
    
    # Find key in target section
    if [[ "$in_section" == true ]] && [[ "$line" =~ ^${key}= ]]; then
      value=$(echo "$line" | cut -d'=' -f2- | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
      echo "$value"
      return 0
    fi
  done < "$file_path"
  
  return 1
}

# Update INI format configuration file
update_ini_config() {
  local file_path="$1"
  local section="$2"
  local key="$3"
  local value="$4"
  
  if [[ ! -f "$file_path" ]]; then
    echo "Error: Config file does not exist: $file_path"
    return 1
  fi
  
  # Check if key exists in specified section
  local in_section=false
  local key_found=false
  local line_number=0
  local target_line=0
  
  while IFS= read -r line; do
    ((line_number++))
    
    # Check if entering target section
    if [[ "$line" =~ ^\[${section}\]$ ]]; then
      in_section=true
      continue
    elif [[ "$line" =~ ^\[.*\]$ ]] && [[ "$in_section" == true ]]; then
      # Entered another section, exit target section
      in_section=false
      continue
    fi
    
    # Find key in target section
    if [[ "$in_section" == true ]] && [[ "$line" =~ ^${key}= ]]; then
      key_found=true
      target_line=$line_number
      break
    fi
  done < "$file_path"
  
  if [[ "$key_found" == true ]]; then
    # Update key-value pair
    sed -i "${target_line}s|^${key}=.*|${key}=${value}|" "$file_path"
    echo "✓ Updated [${section}] ${key} = ${value}"
    return 0
  else
    echo "Warning: Key ${key} not found in [${section}], skipping update"
    return 2
  fi
}

clear_kv_path() {
  local config_file="$1"
  
  if [[ ! -f "$config_file" ]]; then
    echo "Error: Config file does not exist: $config_file"
    return 1
  fi
  
  # Read KVPath from config file
  local kv_path=$(read_ini_value "$config_file" "BuildSSDIndex" "KVPath")
  
  if [[ -z "$kv_path" ]]; then
    echo "KVPath not found in config file, skipping cleanup"
    return 0
  fi
  
  echo "Found KVPath in config: $kv_path"
  
  if [[ ! -d "$kv_path" ]]; then
    echo "Warning: KVPath directory does not exist: $kv_path"
    return 1
  fi
  
  echo "Clearing KVPath directory: $kv_path"
  
  # Remove all files and subdirectories in KVPath
  if rm -rf "${kv_path}"/* 2>/dev/null; then
    echo "✓ KVPath directory cleared successfully"
    return 0
  else
    echo "⚠ Could not clear some files in KVPath (may be empty or permission issues)"
    return 1
  fi
}

# Show current parameter settings
show_parameters() {
    cat << EOF
Parameter Settings:
  Index Directory: $INDEX_DIRECTORY
  Config File: ${INDEX_DIRECTORY}/indexloader.ini
  UBIS Executable: $UBIS_EXECUTABLE

Parameters to modify:
EOF

    [[ -n "$DIM" ]] && echo "  Dim: $DIM"
    [[ -n "$VECTOR_PATH" ]] && echo "  VectorPath: $VECTOR_PATH"
    [[ -n "$QUERY_PATH" ]] && echo "  QueryPath: $QUERY_PATH"
    [[ -n "$HEAD_INDEX_FOLDER" ]] && echo "  HeadIndexFolder: $HEAD_INDEX_FOLDER"
    [[ -n "$POSTING_PAGE_LIMIT" ]] && echo "  PostingPageLimit: $POSTING_PAGE_LIMIT"
    [[ -n "$KV_PATH" ]] && echo "  KVPath: $KV_PATH"
    [[ -n "$RESULT_NUM" ]] && echo "  ResultNum: $RESULT_NUM"
    [[ -n "$START_NUM" ]] && echo "  StartNum: $START_NUM"
    [[ -n "$STEP" ]] && echo "  Step: $STEP"
    [[ -n "$TERMINATE_NUM" ]] && echo "  TerminateNum: $TERMINATE_NUM"
    [[ -n "$INSERT_THREAD_NUM" ]] && echo "  InsertThreadNum: $INSERT_THREAD_NUM"
    [[ -n "$APPEND_THREAD_NUM" ]] && echo "  AppendThreadNum: $APPEND_THREAD_NUM"
    [[ -n "$BALANCE_FACTOR" ]] && echo "  BalanceFactor: $BALANCE_FACTOR"
    [[ -n "$BASE_VECTOR_SPLIT_PATH" ]] && echo "  BaseVectorSplitPath: $BASE_VECTOR_SPLIT_PATH"
    [[ -n "$FULL_VECTOR_PATH" ]] && echo "  FullVectorPath: $FULL_VECTOR_PATH"
    [[ -n "$TRUTH_FILE_PREFIX" ]] && echo "  TruthFilePrefix: $TRUTH_FILE_PREFIX"
    echo ""
}

# Main function
main() {
    # Validate parameters
    validate_parameters
    
    # Derive config file path from index directory
    CONFIG_PATH="${INDEX_DIRECTORY}/indexloader.ini"
    
    echo "Config file path: $CONFIG_PATH"
    echo ""
    
    # Check if config file exists
    if [[ ! -f "$CONFIG_PATH" ]]; then
        echo "Error: Config file does not exist: $CONFIG_PATH"
        echo "Please check the index directory path"
        exit 1
    fi
    
    # Show parameters
    show_parameters
    
    echo "Starting config file update..."
    echo "================================"
    
    # Update individual parameters
    [[ -n "$DIM" ]] && update_ini_config "$CONFIG_PATH" "Base" "Dim" "$DIM"
    [[ -n "$VECTOR_PATH" ]] && update_ini_config "$CONFIG_PATH" "Base" "VectorPath" "$VECTOR_PATH"
    [[ -n "$QUERY_PATH" ]] && update_ini_config "$CONFIG_PATH" "Base" "QueryPath" "$QUERY_PATH"
    [[ -n "$INDEX_DIRECTORY" ]] && update_ini_config "$CONFIG_PATH" "Base" "IndexDirectory" "$INDEX_DIRECTORY"
    [[ -n "$HEAD_INDEX_FOLDER" ]] && update_ini_config "$CONFIG_PATH" "Base" "HeadIndexFolder" "$HEAD_INDEX_FOLDER"
    [[ -n "$POSTING_PAGE_LIMIT" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "PostingPageLimit" "$POSTING_PAGE_LIMIT"
    [[ -n "$KV_PATH" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "KVPath" "$KV_PATH"
    [[ -n "$RESULT_NUM" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "ResultNum" "$RESULT_NUM"
    [[ -n "$START_NUM" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "StartNum" "$START_NUM"
    [[ -n "$STEP" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "Step" "$STEP"
    [[ -n "$TERMINATE_NUM" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "TernimateNum" "$TERMINATE_NUM"
    [[ -n "$INSERT_THREAD_NUM" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "InsertThreadNum" "$INSERT_THREAD_NUM"
    [[ -n "$APPEND_THREAD_NUM" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "AppendThreadNum" "$APPEND_THREAD_NUM"
    [[ -n "$BALANCE_FACTOR" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "BalanceFactor" "$BALANCE_FACTOR"
    [[ -n "$BASE_VECTOR_SPLIT_PATH" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "BaseVectorSplitPath" "$BASE_VECTOR_SPLIT_PATH"
    [[ -n "$FULL_VECTOR_PATH" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "FullVectorPath" "$FULL_VECTOR_PATH"
    [[ -n "$TRUTH_FILE_PREFIX" ]] && update_ini_config "$CONFIG_PATH" "BuildSSDIndex" "TruthFilePrefix" "$TRUTH_FILE_PREFIX"
    
    # Calculate and update derived paths
    echo ""
    echo "Updating derived paths..."
    calculate_derived_paths "$INDEX_DIRECTORY" "$HEAD_INDEX_FOLDER" "$CONFIG_PATH"
    
    echo ""
    echo "Config file update completed!"
    
    # Run UBIS
    run_ubis "$INDEX_DIRECTORY"
    
    # Clear KVPath directory after UBIS execution
    echo ""
    echo "================================"
    echo "Cleaning up KVPath directory..."
    clear_kv_path "$CONFIG_PATH"
}

# Run main function
main "$@"