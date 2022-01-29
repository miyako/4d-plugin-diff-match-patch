![version](https://img.shields.io/badge/version-18%2B-EB8E5F)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-diff-match-patch)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-diff-match-patch/total)

# 4d-plugin-diff-match-patch
Basic implementation of [google-diff-match-patch](https://github.com/google/diff-match-patch)

### Screenshot

<img width="421" alt="2018-02-14 15 49 21" src="https://user-images.githubusercontent.com/1725068/36197663-bed69022-11b7-11e8-8425-3dc7ae9fb62d.png">

instead of standard ``<ins>`` and ``<del>`` tags with ``background`` style, the library is modified to return ``<span>`` with ``background-color`` style, for better compatibility with 4D multi style text.

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
