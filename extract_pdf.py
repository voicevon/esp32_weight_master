import pdfplumber

pdf_path = r"d:\Software\antigravity\esp32_weight_master\doc\7_485通讯伺服驱动器通讯功能的说明及使用.pdf"

try:
    with pdfplumber.open(pdf_path) as pdf:
        for i, page in enumerate(pdf.pages):
            text = page.extract_text()
            if "011F" in text or "0202" in text or "P3-31" in text:
                print(f"--- Page {i+1} ---")
                print(text)
                print("\n")
except Exception as e:
    print(f"Error: {e}")
