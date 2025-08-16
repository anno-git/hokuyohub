# クラスタ点群インデックス追加実装計画

## 目的
DBSCAN2DのCluster構造体に、そのクラスタを構成する元の点群のインデックス情報を追加し、postfilterでの正確な点群操作を可能にする。

## 現在の問題
- postfilterでクラスタ内の点を特定するために、矩形範囲での推定を行っている
- この方法は不正確で、重複するクラスタや密集した点群で誤判定が発生する可能性がある
- 正確な点群操作のためには、元の点群インデックスが必要

## 提案する変更

### 1. Cluster構造体の拡張
**ファイル**: `src/detect/dbscan.h`
```cpp
struct Cluster { 
    uint32_t id; 
    uint8_t sensor_mask; 
    float cx,cy,minx,miny,maxx,maxy; 
    uint16_t count;
    std::vector<size_t> point_indices; // 追加: 元の点群インデックス
};
```

### 2. DBSCAN2D::run()の修正
**ファイル**: `src/detect/dbscan.cpp`
- クラスタリング処理中に各点のインデックスを記録
- Cluster生成時にpoint_indicesを設定
- 既存の統計計算ロジックは維持

### 3. postfilterの簡素化
**ファイル**: `src/detect/postfilter.cpp`
- `applyIsolationRemovalFilter()`から推定ロジックを削除
- `cluster.point_indices`を直接使用
- より正確で効率的な処理が可能

### 4. WebSocket通信の考慮
**ファイル**: `src/io/ws_handlers.cpp`
- `pushClustersLite()`でのJSON変換時にpoint_indicesを含めるかどうか検討
- WebUIでの表示には不要なので、デフォルトでは送信しない

## 影響範囲

### 直接影響
- `src/detect/dbscan.h` - Cluster構造体の定義変更
- `src/detect/dbscan.cpp` - クラスタリングアルゴリズムの修正
- `src/detect/postfilter.cpp` - 点群インデックス推定ロジックの削除

### 間接影響
- `src/io/ws_handlers.cpp` - JSON変換処理（オプション）
- `src/main.cpp` - 特に変更不要（インターフェース互換）

## 実装手順

1. **Cluster構造体の拡張**
   - `point_indices`フィールドを追加
   - デフォルトコンストラクタの調整

2. **DBSCAN2D::run()の修正**
   - クラスタリング中の点インデックス追跡
   - Cluster生成時のpoint_indices設定

3. **postfilterの簡素化**
   - 推定ロジックの削除
   - 直接インデックスアクセスに変更

4. **テストと検証**
   - 既存機能の動作確認
   - postfilterの正確性検証

## メリット
- **正確性**: 点群の正確な特定が可能
- **効率性**: 推定処理が不要になり高速化
- **拡張性**: 将来的な点群操作機能の基盤
- **保守性**: より直感的で理解しやすいコード

## リスク
- **メモリ使用量**: クラスタあたり追加のインデックス配列
- **互換性**: 既存のCluster使用箇所での影響（最小限）

## 代替案
1. **別構造体**: ClusterWithIndicesのような拡張構造体を作成
2. **マップ管理**: cluster_id -> point_indicesのマップを別途管理

推奨は直接Cluster構造体を拡張する案です。シンプルで効率的であり、将来の拡張にも対応しやすいためです。