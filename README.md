# PNG-Decoder

## Description
Program written in C language that decodes the binary content of a PNG file, extracts the pixel-related information, and reconstructs it into an SDL texture.

## Steps
- Reading the contents of a PNG file.
- Verifying the import of a PNG file through “header matching”.
- Reading and parsing the fundamental chunks (IHDR, gAMA, IDAT, IEND).
- Data decompressing with “zlib” library.
- Data reconstruction, using the data from the parsed chunks.
- Initialization and construction of the texture with the reconstructed data. <br>

- <b>Works only with 8 bit true color RGBA and no interlacing, at the time! </b>
