# 4d-plugin-diff-match-patch
Basic implementation of [google-diff-match-patch](https://github.com/google/diff-match-patch)

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
||<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|

### Screenshot

<img width="421" alt="2018-02-14 15 49 21" src="https://user-images.githubusercontent.com/1725068/36197663-bed69022-11b7-11e8-8425-3dc7ae9fb62d.png">

instead of standard ``<ins>`` and ``<del>`` tags with ``background`` style, the library is modified to return ``<span>`` with ``background-color`` style, for better compatibility with 4D multi style text.

### Releases

[1.0](https://github.com/miyako/4d-plugin-diff-match-patch/releases/tag/1.0)

## Syntax

```
diff:=diff_match_patch (old;new{;options})
```

Parameter|Type|Description
------------|------------|----
old|TEXT|
new|TEXT|
options|TEXT|JSON
diff|TEXT|4D styled text (HTML subset)

* Properties of options and thier default values

``diffTimeout``:``1.0``  
``diffEditCost``:``4``  
``patchMargin``:``4``  
``patchDeleteThreshold``:``0.5``  
``matchThreshold``:``0.5``  
``matchDistance``:``1000``  
``matchMaxBits``:``32``  

see [diff_match_patch.h](https://github.com/miyako/4d-plugin-diff-match-patch/blob/master/diff_match_patch/cpp/diff_match_patch.h) for details
