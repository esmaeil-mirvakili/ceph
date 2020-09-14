import { Component } from '@angular/core';

@Component({
  selector: 'cd-bottom-links',
  templateUrl: './bottom-links.component.html',
  styleUrls: ['./bottom-links.component.scss']
})
export class BottomLinksComponent {
  docTitles: any[] = [
    { section: 'security', docText: 'Security' },
    { section: 'help', docText: 'Help' },
    { section: 'trademarks', docText: 'Trademarks' }
  ];
}
