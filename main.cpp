#include <bits/stdc++.h>
using namespace std;

static const char* FILENAME = "storage.dat";
static const uint32_t MAGIC = 0x46534442; // "FSDB"
static const uint32_t VERSION = 1;
static const uint32_t NUM_BUCKETS = 100003;

struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t num_buckets;
};

struct Node {
    uint64_t next;
    int32_t value;
    uint8_t index_len;
    char index[64];
    char _padding[3];
};

static_assert(sizeof(Node) == 80, "Node size must be 80");
static const size_t NODE_SIZE = sizeof(Node);

FILE* g_fp = nullptr;
uint32_t g_num_buckets = 0;
vector<uint64_t> g_bucket_heads;

static size_t fnv1a_hash(const string& s) {
    size_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

static size_t bucket_index(const string& s) {
    return fnv1a_hash(s) % g_num_buckets;
}

static void read_header() {
    Header header;
    fread(&header, sizeof(header), 1, g_fp);
    g_num_buckets = header.num_buckets;
    g_bucket_heads.resize(g_num_buckets);
    fread(g_bucket_heads.data(), sizeof(uint64_t), g_num_buckets, g_fp);
}

static void write_header() {
    Header header;
    header.magic = MAGIC;
    header.version = VERSION;
    header.num_buckets = g_num_buckets;
    fseek(g_fp, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, g_fp);
    fwrite(g_bucket_heads.data(), sizeof(uint64_t), g_num_buckets, g_fp);
}

static void init_file() {
    g_num_buckets = NUM_BUCKETS;
    g_bucket_heads.assign(g_num_buckets, 0);
    write_header();
}

static void open_storage() {
    g_fp = fopen(FILENAME, "r+b");
    if (!g_fp) {
        g_fp = fopen(FILENAME, "w+b");
        if (!g_fp) {
            perror("fopen");
            exit(1);
        }
        init_file();
    } else {
        read_header();
    }
}

static void read_node(uint64_t offset, Node& node) {
    fseek(g_fp, static_cast<long>(offset), SEEK_SET);
    fread(&node, NODE_SIZE, 1, g_fp);
}

static void write_node(uint64_t offset, const Node& node) {
    fseek(g_fp, static_cast<long>(offset), SEEK_SET);
    fwrite(&node, NODE_SIZE, 1, g_fp);
}

static uint64_t allocate_node_offset() {
    fseek(g_fp, 0, SEEK_END);
    return static_cast<uint64_t>(ftell(g_fp));
}

static void update_bucket_head(size_t idx, uint64_t offset) {
    g_bucket_heads[idx] = offset;
    uint64_t pos = sizeof(Header) + idx * sizeof(uint64_t);
    fseek(g_fp, static_cast<long>(pos), SEEK_SET);
    fwrite(&offset, sizeof(uint64_t), 1, g_fp);
}

static bool same_index(const Node& node, const string& index) {
    return node.index_len == index.size() &&
           memcmp(node.index, index.data(), index.size()) == 0;
}

static void do_insert(const string& index, int32_t value) {
    size_t h = bucket_index(index);
    uint64_t curr = g_bucket_heads[h];
    Node node;
    while (curr != 0) {
        read_node(curr, node);
        if (same_index(node, index) && node.value == value) {
            return;
        }
        curr = node.next;
    }
    uint64_t offset = allocate_node_offset();
    Node new_node;
    new_node.next = g_bucket_heads[h];
    new_node.value = value;
    new_node.index_len = static_cast<uint8_t>(index.size());
    memset(new_node.index, 0, sizeof(new_node.index));
    memcpy(new_node.index, index.data(), index.size());
    memset(new_node._padding, 0, sizeof(new_node._padding));
    write_node(offset, new_node);
    update_bucket_head(h, offset);
}

static void do_delete(const string& index, int32_t value) {
    size_t h = bucket_index(index);
    uint64_t prev = 0;
    uint64_t curr = g_bucket_heads[h];
    Node node;
    while (curr != 0) {
        read_node(curr, node);
        if (same_index(node, index) && node.value == value) {
            if (prev == 0) {
                update_bucket_head(h, node.next);
            } else {
                Node prev_node;
                read_node(prev, prev_node);
                prev_node.next = node.next;
                write_node(prev, prev_node);
            }
            return;
        }
        prev = curr;
        curr = node.next;
    }
}

static void do_find(const string& index) {
    size_t h = bucket_index(index);
    vector<int32_t> values;
    values.reserve(8);
    uint64_t curr = g_bucket_heads[h];
    Node node;
    while (curr != 0) {
        read_node(curr, node);
        if (same_index(node, index)) {
            values.push_back(node.value);
        }
        curr = node.next;
    }
    if (values.empty()) {
        cout << "null\n";
    } else {
        sort(values.begin(), values.end());
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) cout << ' ';
            cout << values[i];
        }
        cout << '\n';
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    open_storage();

    int n;
    if (!(cin >> n)) return 0;
    string cmd, index;
    int32_t value;
    for (int i = 0; i < n; ++i) {
        cin >> cmd;
        if (cmd == "insert") {
            cin >> index >> value;
            do_insert(index, value);
        } else if (cmd == "delete") {
            cin >> index >> value;
            do_delete(index, value);
        } else if (cmd == "find") {
            cin >> index;
            do_find(index);
        }
    }

    fclose(g_fp);
    return 0;
}
