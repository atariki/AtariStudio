bool Open(const std::filesystem::path&);
void Close();

bool IsOpen() const;
bool Eof() const;

uint8_t  ReadU8();
uint16_t ReadU16LE();
uint32_t ReadU32LE();

void Read(void* buffer, size_t size);

void Seek(size_t offset);
size_t Tell() const;
size_t Size() const;