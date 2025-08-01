#include <Novice.h>
#include <math.h>
#include <stdio.h>

const char kWindowTitle[] = "LE2D_20_フルヤ_ユウト";
const int kWindowWidth = 1280;
const int kWindowHeight = 720;

// -------- Vector3とMatrix4x4 --------
struct Vector3 {
    float x;
    float y;
    float z;
};

struct Matrix4x4 {
    float m[4][4];
};

// -------- クロス積 --------
Vector3 Cross(const Vector3& v1, const Vector3& v2) {
    Vector3 result;
    result.x = v1.y * v2.z - v1.z * v2.y;
    result.y = v1.z * v2.x - v1.x * v2.z;
    result.z = v1.x * v2.y - v1.y * v2.x;
    return result;
}

// -------- 単位行列 --------
Matrix4x4 MakeIdentityMatrix() {
    Matrix4x4 result{};
    for (int i = 0; i < 4; i++) result.m[i][i] = 1.0f;
    return result;
}

// -------- 射影行列 --------
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ) {
    Matrix4x4 result{};
    float f = 1.0f / tanf(fovY / 2.0f);
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = farZ / (farZ - nearZ);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -nearZ * farZ / (farZ - nearZ);
    return result;
}

// -------- ビューポート行列 --------
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
    Matrix4x4 result{};
    result.m[0][0] = width / 2.0f;
    result.m[1][1] = -height / 2.0f;
    result.m[2][2] = maxDepth - minDepth;
    result.m[3][0] = left + width / 2.0f;
    result.m[3][1] = top + height / 2.0f;
    result.m[3][2] = minDepth;
    result.m[3][3] = 1.0f;
    return result;
}

// -------- アフィン行列 (スケール→Y軸回転→平行移動) --------
Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotate, Vector3 translate) {
    Matrix4x4 result{};

    float cosY = cosf(rotate.y);
    float sinY = sinf(rotate.y);

    result.m[0][0] = scale.x * cosY;
    result.m[0][1] = 0;
    result.m[0][2] = scale.x * -sinY;
    result.m[0][3] = 0;

    result.m[1][0] = 0;
    result.m[1][1] = scale.y;
    result.m[1][2] = 0;
    result.m[1][3] = 0;

    result.m[2][0] = scale.z * sinY;
    result.m[2][1] = 0;
    result.m[2][2] = scale.z * cosY;
    result.m[2][3] = 0;

    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;
    result.m[3][3] = 1;

    return result;
}

// -------- 行列×ベクトル --------
Vector3 Transform(const Vector3& v, const Matrix4x4& m) {
    Vector3 result;
    float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
    result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];

    if (w != 0.0f) {
        result.x /= w;
        result.y /= w;
        result.z /= w;
    }

    return result;
}

// -------- 行列×行列 --------
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result{};
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            for (int k = 0; k < 4; ++k)
                result.m[y][x] += m1.m[y][k] * m2.m[k][x];
    return result;
}

// -------- ビュー行列の逆行列（アフィン用） --------
Matrix4x4 Inverse(const Matrix4x4& m) {
    Matrix4x4 result = MakeIdentityMatrix();

    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            result.m[y][x] = m.m[x][y];

    result.m[3][0] = -(m.m[3][0] * result.m[0][0] + m.m[3][1] * result.m[1][0] + m.m[3][2] * result.m[2][0]);
    result.m[3][1] = -(m.m[3][0] * result.m[0][1] + m.m[3][1] * result.m[1][1] + m.m[3][2] * result.m[2][1]);
    result.m[3][2] = -(m.m[3][0] * result.m[0][2] + m.m[3][1] * result.m[1][2] + m.m[3][2] * result.m[2][2]);

    return result;
}

// -------- デバッグ用出力 --------
void VectorScreenPrintf(int x, int y, const Vector3& v, const char* label) {
    char text[128];
    sprintf_s(text, "%s: (%.2f, %.2f, %.2f)", label, v.x, v.y, v.z);
    Novice::ScreenPrintf(x, y, text);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Novice::Initialize(kWindowTitle, kWindowWidth, kWindowHeight);
    char keys[256]{};
    char preKeys[256]{};

    Vector3 rotate{};
    Vector3 translate{};
    Vector3 cameraPos{ 0, 0, -5 };

    Vector3 v1{ 1.2f, -3.9f, 2.5f };
    Vector3 v2{ 2.8f, 0.4f, -1.3f };

    Vector3 triangleVertices[3] = {
        { 0.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f },
        { 1.0f, -1.0f, 0.0f }
    };

    while (Novice::ProcessMessage() == 0) {
        Novice::BeginFrame();
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

        // 自動回転
        rotate.y += 0.03f;

        // 回転方向に応じた移動ベクトル計算
        float angle = rotate.y;
        float forwardZ = cosf(angle);
      

        // 入力に応じて移動（xとzを独立）
        // 前後移動（Z軸方向）
      // 入力に応じて移動（xとzを独立）
// 前後移動（Z軸方向）
        if (keys[DIK_W]) {
            translate.z += forwardZ * 0.1f;
        }
        if (keys[DIK_S]) {
            translate.z -= forwardZ * 0.1f;
        }

        // 左右移動（X軸方向） → 回転の影響を受けずに単純移動
        if (keys[DIK_A]) {
            translate.x -= 0.1f;
        }
        if (keys[DIK_D]) {
            translate.x += 0.1f;
        }

        // クロス積の確認
        Vector3 cross = Cross(v1, v2);
        VectorScreenPrintf(0, 0, cross, "Cross");

        // 行列の計算
        Matrix4x4 worldMatrix = MakeAffineMatrix({ 1, 1, 1 }, rotate, translate);
        Matrix4x4 cameraMatrix = MakeAffineMatrix({ 1, 1, 1 }, { 0, 0, 0 }, cameraPos);
        Matrix4x4 viewMatrix = Inverse(cameraMatrix);
        Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kWindowWidth) / float(kWindowHeight), 0.1f, 100.0f);
        Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
        Matrix4x4 viewportMatrix = MakeViewportMatrix(0, 0, float(kWindowWidth), float(kWindowHeight), 0.0f, 1.0f);

        // 画面座標に変換
        Vector3 screenVertices[3];
        for (int i = 0; i < 3; ++i) {
            Vector3 ndc = Transform(triangleVertices[i], wvpMatrix);
            screenVertices[i] = Transform(ndc, viewportMatrix);
        }

        // 描画
        Novice::DrawTriangle(
            int(screenVertices[0].x), int(screenVertices[0].y),
            int(screenVertices[1].x), int(screenVertices[1].y),
            int(screenVertices[2].x), int(screenVertices[2].y),
            RED, kFillModeSolid
        );

        Novice::EndFrame();
        if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) break;
    }

    Novice::Finalize();
    return 0;
}