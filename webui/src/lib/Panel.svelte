<script>
  let { title, width, height, flex = false, children } = $props();
</script>

<div
  class="flex flex-col border-r border-border overflow-hidden"
  style:width={!flex && width ? width : undefined}
  style:min-width={!flex && width ? width : undefined}
  style:height={!flex && height ? height : undefined}
  style:min-height={!flex && height ? height : undefined}
  style:flex={flex ? '1' : undefined}
>
  <div class="flex items-center h-7 px-3 bg-panel-light border-b border-border shrink-0">
    <span class="text-xs font-medium text-text-dim">{title}</span>
  </div>

  <div class="flex-1 overflow-auto">
    {@render children()}
  </div>
</div>