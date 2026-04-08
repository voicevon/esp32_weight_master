import pdfplumber

pdf_path = r"d:\Software\antigravity\esp32_weight_master\doc\7_485通讯伺服驱动器通讯功能的说明及使用.pdf"

with pdfplumber.open(pdf_path) as pdf:
    with open("manual_text.txt", "w", encoding="utf-8") as f:
        for p in pdf.pages:
            f.write(p.extract_text() or "")
            f.write("\n\n")
