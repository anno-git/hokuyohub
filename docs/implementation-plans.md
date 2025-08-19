# HokuyoHub 実装方針集

この文書は、現在のTODO一覧の各項目について実装の方針を示します。具体実装は担当者に委ねます。
作業担当者は実装計画を日本語でdocs/plansディレクトリにmdファイルで作成し、管理者の承認を得てから作業を開始してください。

TODO項目
- レイアウト修正


---

レイアウト修正

- 以下のものをCanvasの上部に重ねて表示
   - Show Raw Points, Show Filtered Data, Per-sensor Colors
   - Viewport: Reset View, Scale
   - ROI: + Include Region, + Exclude Region, Clear All ROI
- Filter Configurationの各グループをアコーディオン式に畳めるようにする
   - その際、enable/disableの操作は畳んだ状態でもできるようにする