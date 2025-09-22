# Default configuration variables
DEFAULT_DATASET_NAME="cohere1m"
DEFAULT_DATASET_PATH="$HOME/datasets/$DEFAULT_DATASET_NAME"
DEFAULT_DATASET_BATCH_PATH="$DEFAULT_DATASET_PATH/batches"
DEFAULT_USEFULTOOL_PATH="$HOME/UBIS/Release/usefultool"

# Default parameter values
DEFAULT_BATCH=10
DEFAULT_VECTOR_TYPE="float"
DEFAULT_BASE_VECTOR_PATH="$DEFAULT_DATASET_PATH/base_embeddings.bin"
DEFAULT_CURRENT_LIST_FILE="$DEFAULT_DATASET_PATH/batches/base_embedding_ids"
DEFAULT_QUERY_VECTOR_PATH="$DEFAULT_DATASET_PATH/query_embeddings.bin"
DEFAULT_BASE_VECTOR_SPLIT_PATH="$DEFAULT_DATASET_PATH/query_vector_range.bin"
DEFAULT_FILETYPE="DEFAULT"
DEFAULT_RIDE=10
DEFAULT_DIMENSION=128
DEFAULT_FORMAT="DEFAULT"
DEFAULT_NEW_DATASET_FILE="$DEFAULT_DATASET_BATCH_PATH/base_embeddings"
DEFAULT_NEW_QUERY_DATASET_FILE="$DEFAULT_DATASET_BATCH_PATH/query_embeddings"
DEFAULT_CONFIG_PATH=""
DEFAULT_ONLY_CONFIG=false

# Display help information
show_help() {
    cat << EOF
Usage: $0 [options]

Options:
  -D, --datasetname NAME                 Dataset name (default: $DEFAULT_DATASET_NAME)
  -b, --batch BATCH                      Batch size (default: $DEFAULT_BATCH)
  -t, --vectortype TYPE                  Vector type (default: $DEFAULT_VECTOR_TYPE)
  -B, --basevectorpath PATH              Base vector path (default: $DEFAULT_BASE_VECTOR_PATH)
  -L, --currentlistfile FILE             Current list file (default: $DEFAULT_CURRENT_LIST_FILE)
  -Q, --queryvectorpath PATH             Query vector path (default: $DEFAULT_QUERY_VECTOR_PATH)
  -S, --basevectorsplitpath PATH         Base vector split path (default: $DEFAULT_BASE_VECTOR_SPLIT_PATH)
  -F, --filetype TYPE                    File type (default: $DEFAULT_FILETYPE)
  -r, --ride RIDE                        Ride parameter (default: $DEFAULT_RIDE)
  -d, --dimension DIM                    Dimension (default: $DEFAULT_DIMENSION)
  -f, --format FORMAT                    Format (default: $DEFAULT_FORMAT)
  -N, --newdatasetfile FILE              New dataset file (default: $DEFAULT_NEW_DATASET_FILE)
  -q, --newquerydatasetfile FILE         New query dataset file (default: $DEFAULT_NEW_QUERY_DATASET_FILE)
  -c, --configpath PATH                  Configuration file path (optional)
  -o, --onlyconfig BOOLEAN               Only execute the third run with config (true/false) (default: $DEFAULT_ONLY_CONFIG)
  -h, --help                             Show this help message

Examples:
  $0 -D mydataset -o false -b 10 -t float -B /path/to/base.bin -Q /path/to/query.bin
  $0 --datasetname mydata --onlyconfig false --batch 5 --vectortype float --configpath \$HOME/config.ini
EOF
}

# Initialize parameter variables
DATASET_NAME=$DEFAULT_DATASET_NAME
BATCH=$DEFAULT_BATCH
VECTOR_TYPE=$DEFAULT_VECTOR_TYPE
BASE_VECTOR_PATH=$DEFAULT_BASE_VECTOR_PATH
CURRENT_LIST_FILE=$DEFAULT_CURRENT_LIST_FILE
QUERY_VECTOR_PATH=$DEFAULT_QUERY_VECTOR_PATH
BASE_VECTOR_SPLIT_PATH=$DEFAULT_BASE_VECTOR_SPLIT_PATH
FILETYPE=$DEFAULT_FILETYPE
RIDE=$DEFAULT_RIDE
DIMENSION=$DEFAULT_DIMENSION
FORMAT=$DEFAULT_FORMAT
NEW_DATASET_FILE=$DEFAULT_NEW_DATASET_FILE
NEW_QUERY_DATASET_FILE=$DEFAULT_NEW_QUERY_DATASET_FILE
CONFIG_PATH=$DEFAULT_CONFIG_PATH
ONLY_CONFIG=$DEFAULT_ONLY_CONFIG

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -D|--datasetname)
                DATASET_NAME="$2"
                shift 2
                ;;
            -b|--batch)
                BATCH="$2"
                shift 2
                ;;
            -t|--vectortype)
                VECTOR_TYPE="$2"
                shift 2
                ;;
            -B|--basevectorpath)
                BASE_VECTOR_PATH="$2"
                shift 2
                ;;
            -L|--currentlistfile)
                CURRENT_LIST_FILE="$2"
                shift 2
                ;;
            -Q|--queryvectorpath)
                QUERY_VECTOR_PATH="$2"
                shift 2
                ;;
            -S|--basevectorsplitpath)
                BASE_VECTOR_SPLIT_PATH="$2"
                shift 2
                ;;
            -F|--filetype)
                FILETYPE="$2"
                shift 2
                ;;
            -r|--ride)
                RIDE="$2"
                shift 2
                ;;
            -d|--dimension)
                DIMENSION="$2"
                shift 2
                ;;
            -f|--format)
                FORMAT="$2"
                shift 2
                ;;
            -N|--newdatasetfile)
                NEW_DATASET_FILE="$2"
                shift 2
                ;;
            -q|--newquerydatasetfile)
                NEW_QUERY_DATASET_FILE="$2"
                shift 2
                ;;
            -c|--configpath)
                CONFIG_PATH="$2"
                shift 2
                ;;
            -o|--onlyconfig)
                # true/false, yes/no, 1/0
                case "$(echo "$2" | tr '[:upper:]' '[:lower:]')" in
                    true|yes|1)
                        ONLY_CONFIG=true
                        ;;
                    false|no|0)
                        ONLY_CONFIG=false
                        ;;
                    *)
                        echo "Error: Invalid value for --onlyconfig: $2"
                        echo "       Use true/false, yes/no, or 1/0"
                        exit 1
                        ;;
                esac
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo "Error: Unknown option $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Calculate derived paths based on dataset name
calculate_paths() {
    DATASET_PATH="$HOME/datasets/$DATASET_NAME"
    DATASET_BATCH_PATH="$DATASET_PATH/batches"
    USEFULTOOL_PATH="$HOME/UBIS/Release/usefultool"
    
    # Update paths that depend on DATASET_NAME if they still have default values
    if [[ "$BASE_VECTOR_PATH" == "$DEFAULT_BASE_VECTOR_PATH" ]]; then
        BASE_VECTOR_PATH="$DATASET_PATH/base_embeddings.bin"
    fi
    
    if [[ "$CURRENT_LIST_FILE" == "$DEFAULT_CURRENT_LIST_FILE" ]]; then
        CURRENT_LIST_FILE="$DATASET_PATH/batches/base_embedding_ids"
    fi
    
    if [[ "$QUERY_VECTOR_PATH" == "$DEFAULT_QUERY_VECTOR_PATH" ]]; then
        QUERY_VECTOR_PATH="$DATASET_PATH/query_embeddings.bin"
    fi
    
    if [[ "$BASE_VECTOR_SPLIT_PATH" == "$DEFAULT_BASE_VECTOR_SPLIT_PATH" ]]; then
        BASE_VECTOR_SPLIT_PATH="$DATASET_PATH/query_vector_range.bin"
    fi
    
    if [[ "$NEW_DATASET_FILE" == "$DEFAULT_NEW_DATASET_FILE" ]]; then
        NEW_DATASET_FILE="$DATASET_BATCH_PATH/base_embeddings"
    fi
    
    if [[ "$NEW_QUERY_DATASET_FILE" == "$DEFAULT_NEW_QUERY_DATASET_FILE" ]]; then
        NEW_QUERY_DATASET_FILE="$DATASET_BATCH_PATH/query_embeddings"
    fi
}

# Display current configuration
show_config() {
    echo "Current configuration:"
    echo "  Dataset Name: $DATASET_NAME"
    echo "  Dataset Path: $DATASET_PATH"
    echo "  Dataset Batch Path: $DATASET_BATCH_PATH"
    echo "  UsefulTool Path: $USEFULTOOL_PATH"
    echo "  Batch: $BATCH"
    echo "  Vector Type: $VECTOR_TYPE"
    echo "  Base Vector Path: $BASE_VECTOR_PATH"
    echo "  Current List File: $CURRENT_LIST_FILE"
    echo "  Query Vector Path: $QUERY_VECTOR_PATH"
    echo "  Base Vector Split Path: $BASE_VECTOR_SPLIT_PATH"
    echo "  Filetype: $FILETYPE"
    echo "  Ride: $RIDE"
    echo "  Dimension: $DIMENSION"
    echo "  Format: $FORMAT"
    echo "  New Dataset File: $NEW_DATASET_FILE"
    echo "  New Query Dataset File: $NEW_QUERY_DATASET_FILE"
    echo "  Config Path: ${CONFIG_PATH:-Not set}"
    echo "  Only Config Execution: $ONLY_CONFIG"
    echo ""
}

# Execute usefultool command
run_usefultool() {
    local batch_val="$1"
    local config_path="$2"
    
    echo "================================================"
    echo "Executing command: Batch=$batch_val, Config=${config_path:-Not set}"
    echo "================================================"
    
    # Build command arguments
    local cmd_args=(
        "-BuildVector" "true"
        "--vectortype" "$VECTOR_TYPE"
        "--BaseVectorPath" "$BASE_VECTOR_PATH"
        "--CurrentListFileName" "$CURRENT_LIST_FILE"
        "--QueryVectorPath" "$QUERY_VECTOR_PATH"
        "--BaseVectorSplitPath" "$BASE_VECTOR_SPLIT_PATH"
        "--filetype" "$FILETYPE"
        "--ride" "$RIDE"
        "-d" "$DIMENSION"
        "-f" "$FORMAT"
        "--Batch" "$batch_val"
        "-NewDataSetFileName" "$NEW_DATASET_FILE"
        "-NewQueryDataSetFileName" "$NEW_QUERY_DATASET_FILE"
    )
    
    # Only add ConfigurePath parameter if config_path is not empty
    if [[ -n "$config_path" ]]; then
        cmd_args+=("-ConfigurePath" "$config_path")
    fi
    
    # Execute command
    echo "Executing command: $USEFULTOOL_PATH ${cmd_args[@]}"
    echo ""
    
    # Actually execute the command
    "$USEFULTOOL_PATH" "${cmd_args[@]}"
    
    # Check command execution result
    if [ $? -eq 0 ]; then
        echo "Command executed successfully!"
    else
        echo "Error: Command execution failed!"
        exit 1
    fi
    echo ""
}

# Main function
main() {
    # Parse command line arguments
    parse_arguments "$@"
    
    # Calculate paths based on dataset name
    calculate_paths
    
    # Display configuration
    show_config

    if [ ! -d "$DATASET_BATCH_PATH" ]; then
        mkdir -p "$DATASET_BATCH_PATH"
        echo "Folder created: $DATASET_BATCH_PATH"
    else
        echo "Folder exists: $DATASET_BATCH_PATH"
    fi
    
    echo "Starting usefultool tasks..."
    echo ""
    
    if [ "$ONLY_CONFIG" = true ]; then
        
        if [[ -n "$CONFIG_PATH" ]]; then
            echo "=== Only executing third execution with config file ==="
            run_usefultool "$BATCH" "$CONFIG_PATH"
        else
            echo "Error: --onlyconfig is true but no config file path provided!"
            echo "       Please provide a config file path using -c or --configpath"
            exit 1
        fi
    else
        
        # First execution: batch=0, no ConfigurePath
        echo "=== First execution: batch=0, no config file ==="
        run_usefultool "0" ""
        
        # Wait a moment to ensure first execution completes
        sleep 2
        
        # Second execution: use specified batch number, no ConfigurePath
        echo "=== Second execution: batch=$BATCH, no config file ==="
        run_usefultool "$BATCH" ""
        
        # Wait a moment to ensure second execution completes
        sleep 2
        
        # Third execution: use given ConfigurePath, batch uses specified value
        if [[ -n "$CONFIG_PATH" ]]; then
            echo "=== Third execution: batch=$BATCH, with config file ==="
            run_usefultool "$BATCH" "$CONFIG_PATH"
        else
            echo "=== Third execution: config file path not provided, skipping ==="
        fi
    fi
    
    echo "All tasks completed successfully!"
}

# Check if help parameter is provided
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    show_help
    exit 0
fi

# Run main function
main "$@"