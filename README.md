# 4d-plugin-diff-match-patch
Basic implementation of [google-diff-match-patch](https://github.com/google/diff-match-patch)

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|

### Version

<img src="https://cloud.githubusercontent.com/assets/1725068/18940649/21945000-8645-11e6-86ed-4a0f800e5a73.png" width="32" height="32" /> <img src="https://cloud.githubusercontent.com/assets/1725068/18940648/2192ddba-8645-11e6-864d-6d5692d55717.png" width="32" height="32" />

### Screenshot

<img width="421" alt="2018-02-14 15 49 21" src="https://user-images.githubusercontent.com/1725068/36197663-bed69022-11b7-11e8-8425-3dc7ae9fb62d.png">

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

see []() for details
