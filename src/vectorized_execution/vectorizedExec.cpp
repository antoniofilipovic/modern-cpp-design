

#include <cstdio>
#include <functional>
#include <iostream>
#include <vector>

struct Chunk {
    std::vector<int> &getStorage();
};

struct VectorChunk : public Chunk {

    std::vector<int> &getStorage() override {
        return storage_;
    }

    std::vector<int> storage_;
};

class Data {
public:
    Data() {

    }

    bool hasNext() {
        return current_ < data_.size();
    }

    Chunk getData() {
        return std::move(data_[current_++]);
    }
private:
    std::vector<Chunk> data_;
    std::size_t current_{0};
};





class IOperator {
public:
    void process(Chunk );
};


class Scan : public IOperator {
public:
    Scan(const Data& data): data_(data) {

    }

    void process([[maybe_unused]] Chunk data) override {
        while (data_.hasNext()) {
            Chunk chunk = data_.getData();
            downstream_.process(std::move(chunk));
        }

    }

private:
    Data& data_;
    IOperator &downstream_;

};


class Filter {
public:
    Filter(Scan &scan) {

    }

    void process(Chunk chunk) {
        Chunk processedChunk = filter_(chunk);

        downstream_.process(std::move(processedChunk));


    }
private:
    IOperator &downstream_;

    std::function<Chunk(Chunk &)> filter_;
    IOperator &upstream_;
};

class Output {
public:
    void process(Chunk chunk) {
        if (!chunk.hasValue()) {

        }
        for (auto i : chunk.getStorage()) {
            std::cout << i << " ";
        }
    }
};

int main() {

}