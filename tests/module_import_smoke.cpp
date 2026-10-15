import matrixbench.determinant;

using DefaultMatrix = matrixbench::Matrix<double>;
using JaggedMatrix = matrixbench::Matrix<double, matrixbench::JaggedStorage>;

static_assert(sizeof(DefaultMatrix) > 0);
static_assert(sizeof(JaggedMatrix) > 0);

int main() {
    return 0;
}
