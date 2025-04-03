import pandas as pd

# Load the CSV file from the repo
file_path = "dataset/youtube_urls.csv"

try:
    df = pd.read_csv(file_path)


    print("Original Data:")
    print(df.head())



    # 1. Ensure correct column names
    expected_columns = ["File", "YouTube_URL"]
    if list(df.columns) != expected_columns:
        print("Fixing column names...")
        df.columns = expected_columns[:len(df.columns)]  

    # 2. Remove empty rows
    df.dropna(inplace=True)

    # 3. Remove duplicate entries
    df.drop_duplicates(inplace=True)

    # 4. Ensure URLs are correctly formatted
    df = df[df["YouTube_URL"].str.startswith("http", na=False)]

    # 5. Save the cleaned CSV file
    fixed_file_path = "dataset/youtube_urls_fixed.csv"
    df.to_csv(fixed_file_path, index=False, encoding="utf-8")

    print(f" Fixed file saved as: {fixed_file_path}")

except Exception as e:
    print(f" Error processing the file: {e}")
